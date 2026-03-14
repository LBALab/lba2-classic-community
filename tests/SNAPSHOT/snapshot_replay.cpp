/*
 * snapshot_replay.cpp — Shared replay logic for snapshot testing
 *
 * Loads a .lba2snap file, applies astate to rendering globals,
 * renders all objects, and writes the raw framebuffer to a file.
 *
 * Used by both replay_snapshot_asm and replay_snapshot_cpp.
 * The only difference is which library (ASM or CPP) is linked.
 */

#include "snapshot_replay.h"

#include <SNAPSHOT/SNAPSHOT.H>
#include <POLYGON/POLY.H>
#include <3D/CAMERA.H>
#include <3D/LIGHT.H>
#include <3D/DATAMAT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>
#include <SVGA/CLIP.H>
#include <OBJECT/AFF_OBJ.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  FRAMEBUFFER ALLOCATION                                                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

static U8  *framebuffer = NULL;
static U16 *zbuffer = NULL;
static U32  fb_size = 0;
static U32  zb_size = 0;

static int allocate_buffers(U32 width, U32 height) {
    fb_size = width * height;  /* 8-bit indexed color */
    zb_size = width * height * sizeof(U16);

    framebuffer = (U8 *)calloc(1, fb_size);
    zbuffer = (U16 *)calloc(1, zb_size);

    if (!framebuffer || !zbuffer) {
        fprintf(stderr, "ERROR: Failed to allocate framebuffer/zbuffer (%u x %u)\n",
                width, height);
        return -1;
    }

    /* Point the rendering engine at our buffers */
    Log = framebuffer;
    PtrZBuffer = zbuffer;

    return 0;
}

static void free_buffers(void) {
    if (framebuffer) { free(framebuffer); framebuffer = NULL; }
    if (zbuffer)     { free(zbuffer);     zbuffer = NULL; }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  OBJECT RENDERING                                                          */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void render_object(const T_SCENE_SNAPSHOT *snap, U32 obj_idx) {
    const T_SNAPSHOT_OBJECT *obj = &snap->objects[obj_idx];

    /* Set body pointer as texture/body data from snapshot */
    void *body_ptr = NULL;
    if (obj->body_index < snap->num_bodies) {
        body_ptr = snap->body_data[obj->body_index];
    }
    if (!body_ptr) return;

    /* Set texture page */
    if (obj->texture_index < snap->num_textures) {
        PtrMap = snap->texture_data[obj->texture_index];
        ObjPtrMap = snap->texture_data[obj->texture_index];
    }

    if (obj->render_type == 1 && obj->has_animation) {
        /* ObjectDisplay: build a T_OBJ_3D on the stack */
        T_OBJ_3D obj3d;
        U32 g;

        memset(&obj3d, 0, sizeof(obj3d));

        obj3d.X = obj->X;
        obj3d.Y = obj->Y;
        obj3d.Z = obj->Z;
        obj3d.Alpha = obj->Alpha;
        obj3d.Beta = obj->Beta;
        obj3d.Gamma = obj->Gamma;

        obj3d.Body.Ptr = body_ptr;
        obj3d.Texture = ObjPtrMap;

        obj3d.LastFrame = obj->LastFrame;
        obj3d.NextFrame = obj->NextFrame;
        obj3d.LoopFrame = obj->LoopFrame;
        obj3d.LastTimer = obj->LastTimer;
        obj3d.NextTimer = obj->NextTimer;
        obj3d.NbFrames = obj->NbFrames;
        obj3d.NbGroups = obj->NbGroups;
        obj3d.LastNbGroups = obj->LastNbGroups;
        obj3d.NextNbGroups = obj->NextNbGroups;
        obj3d.Interpolator = obj->Interpolator;
        obj3d.Time = obj->Time;
        obj3d.Status = obj->Status;
        obj3d.Master = obj->Master;
        obj3d.LastAnimStepX = obj->LastAnimStepX;
        obj3d.LastAnimStepY = obj->LastAnimStepY;
        obj3d.LastAnimStepZ = obj->LastAnimStepZ;
        obj3d.LastAnimStepAlpha = obj->LastAnimStepAlpha;
        obj3d.LastAnimStepBeta = obj->LastAnimStepBeta;
        obj3d.LastAnimStepGamma = obj->LastAnimStepGamma;
        obj3d.LastOfsIsPtr = obj->LastOfsIsPtr;

        for (g = 0; g < 30; g++) {
            obj3d.CurrentFrame[g].Type = obj->CurrentFrame_Type[g];
            obj3d.CurrentFrame[g].Alpha = obj->CurrentFrame_Alpha[g];
            obj3d.CurrentFrame[g].Beta = obj->CurrentFrame_Beta[g];
            obj3d.CurrentFrame[g].Gamma = obj->CurrentFrame_Gamma[g];
        }

        /* Disable TransFctBody — body pointer is already resolved */
        TransFctBody = NULL;

        ObjectDisplay(&obj3d);
    } else {
        /* BodyDisplay: static body at given position/rotation */
        BodyDisplay(obj->X, obj->Y, obj->Z,
                    obj->Alpha, obj->Beta, obj->Gamma,
                    body_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  MAIN REPLAY ENTRY POINT                                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

int snapshot_replay_run(const char *snapshot_file, const char *output_file) {
    T_SCENE_SNAPSHOT snap;
    FILE *f;
    U32 i;

    /* Load snapshot */
    if (Snapshot_Load(snapshot_file, &snap) != 0) {
        fprintf(stderr, "ERROR: Failed to load snapshot '%s'\n", snapshot_file);
        return 1;
    }

    /* Apply all rendering globals */
    Snapshot_ApplyState(&snap);

    /* Allocate framebuffer and Z-buffer */
    if (allocate_buffers(snap.shared.ModeDesiredX, snap.shared.ModeDesiredY) != 0) {
        Snapshot_Free(&snap);
        return 1;
    }

    /* Also allocate and set up CLUTs if they came from the snapshot */
    if (snap.clut_gouraud) {
        PtrCLUTGouraud = snap.clut_gouraud;
    }
    if (snap.clut_fog) {
        PtrCLUTFog = snap.clut_fog;
    }

    /* Set up palette */
    PtrTruePal = snap.palette;
    memcpy(Fill_Logical_Palette, snap.logical_palette, SNAPSHOT_PALETTE_SIZE);

    /* Render all objects in sort order (or sequential if no sort order captured) */
    if (snap.sort_order && snap.sort_order_count > 0) {
        for (i = 0; i < snap.sort_order_count; i++) {
            U32 idx = snap.sort_order[i];
            if (idx < snap.num_objects) {
                render_object(&snap, idx);
            }
        }
    } else {
        for (i = 0; i < snap.num_objects; i++) {
            render_object(&snap, i);
        }
    }

    /* Write framebuffer to output file */
    f = fopen(output_file, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Failed to open output file '%s'\n", output_file);
        free_buffers();
        Snapshot_Free(&snap);
        return 1;
    }
    fwrite(framebuffer, 1, fb_size, f);
    fclose(f);

    /* Cleanup */
    free_buffers();
    Snapshot_Free(&snap);

    return 0;
}
