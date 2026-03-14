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
    zbuffer = (U16 *)malloc(zb_size);

    if (!framebuffer || !zbuffer) {
        fprintf(stderr, "ERROR: Failed to allocate framebuffer/zbuffer (%u x %u)\n",
                width, height);
        return -1;
    }

    /* Initialize Z-buffer to maximum (far plane) so new pixels pass Z-test */
    memset(zbuffer, 0xFF, zb_size);

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
    if (!body_ptr) {
        fprintf(stderr, "  obj[%u]: SKIP (no body)\n", obj_idx);
        return;
    }

    fprintf(stderr, "  obj[%u]: pos=(%d,%d,%d) rot=(%d,%d,%d) body_idx=%u body_size=%u type=%d\n",
            obj_idx, obj->X, obj->Y, obj->Z,
            obj->Alpha, obj->Beta, obj->Gamma,
            obj->body_index, obj->body_data_size, obj->render_type);

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
/*  DEBUG: pixel trace callback for filler instrumentation                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* Function pointer defined in POLY.CPP (when SNAPSHOT_DEBUG_PIXEL is set) */
extern void (*snapshot_debug_pixel_fn)(U32, U8, U32, U32, S32, S32, S32, S32, S32, S32);

static void debug_pixel_callback(U32 polyType, U8 polyColor, U32 nbPoints,
    U32 unused, S32 pixelBefore, S32 pixelAfter, S32 r1, S32 r2, S32 r3, S32 r4) {
    fprintf(stderr, "DEBUG PIXEL: poly_type=%u color=%u npts=%u before=%d after=%d\n",
            polyType, polyColor, nbPoints, pixelBefore, pixelAfter);
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  PPM OUTPUT                                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */

static int write_ppm(const char *path, const U8 *fb, U32 w, U32 h,
                     const U8 *rgb_palette) {
    FILE *f = fopen(path, "wb");
    U32 i, npix;
    if (!f) {
        fprintf(stderr, "ERROR: Failed to open PPM output '%s'\n", path);
        return -1;
    }
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    npix = w * h;
    for (i = 0; i < npix; i++) {
        U8 idx = fb[i];
        fputc(rgb_palette[idx * 3 + 0], f);  /* R */
        fputc(rgb_palette[idx * 3 + 1], f);  /* G */
        fputc(rgb_palette[idx * 3 + 2], f);  /* B */
    }
    fclose(f);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  MAIN REPLAY ENTRY POINT                                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

int snapshot_replay_run(const char *snapshot_file, const char *output_file,
                        const char *ppm_file, const char *ref_ppm_file,
                        int only_object) {
    T_SCENE_SNAPSHOT snap;
    FILE *f;
    U32 i;

    /* Load snapshot */
    if (Snapshot_Load(snapshot_file, &snap) != 0) {
        fprintf(stderr, "ERROR: Failed to load snapshot '%s'\n", snapshot_file);
        return 1;
    }

    /* Enable pixel debug tracing */
    snapshot_debug_pixel_fn = debug_pixel_callback;

    fprintf(stderr, "INFO: Loaded snapshot: %ux%u, %u objects, %u bodies, %u textures\n",
            snap.shared.ModeDesiredX, snap.shared.ModeDesiredY,
            snap.num_objects, snap.num_bodies, snap.num_textures);

    /* Allocate framebuffer and Z-buffer BEFORE applying state */
    if (allocate_buffers(snap.shared.ModeDesiredX, snap.shared.ModeDesiredY) != 0) {
        Snapshot_Free(&snap);
        return 1;
    }

    /* Rebuild TabOffLine for our framebuffer instead of using the captured one.
     * The captured values are offsets into the original game's framebuffer but
     * should match (both are width*y) unless the game used a non-standard pitch. */
    {
        U32 y;
        for (y = 0; y < snap.shared.ModeDesiredY && y < 1024; y++) {
            TabOffLine[y] = y * snap.shared.ModeDesiredX;
        }
        PTR_TabOffLine = TabOffLine;
        ScreenPitch = snap.shared.ModeDesiredX;
    }

    /* Apply all rendering globals from the snapshot */
    Snapshot_ApplyState(&snap);

    /* Re-set Log to OUR framebuffer (ApplyState doesn't touch Log) */
    Log = framebuffer;
    PtrZBuffer = zbuffer;

    /* Set up CLUTs — point directly at snapshot data */
    if (snap.clut_gouraud) {
        PtrCLUTGouraud = snap.clut_gouraud;
    }
    if (snap.clut_fog) {
        PtrCLUTFog = snap.clut_fog;
    }

    /* Set up palette mapping */
    PtrTruePal = snap.palette;
    memcpy(Fill_Logical_Palette, snap.logical_palette, SNAPSHOT_PALETTE_SIZE);

    /* CRITICAL: Initialize the polygon fill dispatch table.
     * Without this, the rendering engine doesn't know which fillers to use. */
    Switch_Fillers(FILL_POLY_TEXTURES);

    /* Set fog if fog flag is enabled */
    if (snap.shared.Fill_Flag_Fog) {
        SetFog(snap.shared.Fill_Z_Fog_Near, snap.shared.Fill_Z_Fog_Far);
    }

    fprintf(stderr, "INFO: Rendering %u objects (ZBuf=%u, Fog=%u, Pitch=%u)\n",
            snap.num_objects,
            snap.shared.Fill_Flag_ZBuffer,
            snap.shared.Fill_Flag_Fog,
            ScreenPitch);

    /* Render all objects in sort order (or sequential if no sort order captured) */
    if (snap.sort_order && snap.sort_order_count > 0) {
        for (i = 0; i < snap.sort_order_count; i++) {
            U32 idx = snap.sort_order[i];
            if (idx < snap.num_objects) {
                U8 before = framebuffer[111674];
                if (only_object >= 0 && idx != (U32)only_object) continue;
                render_object(&snap, idx);
                if (framebuffer[111674] != before) {
                    fprintf(stderr, "INFO: obj[%u] changed pixel 111674: %u -> %u\n",
                            idx, before, framebuffer[111674]);
                }
            }
        }
    } else {
        for (i = 0; i < snap.num_objects; i++) {
            U8 before = framebuffer[111674];
            if (only_object >= 0 && i != (U32)only_object) continue;
            render_object(&snap, i);
            if (framebuffer[111674] != before) {
                fprintf(stderr, "INFO: obj[%u] changed pixel 111674: %u -> %u\n",
                        i, before, framebuffer[111674]);
            }
        }
    }

    /* Count non-zero pixels in framebuffer */
    {
        U32 nonzero = 0, total = fb_size;
        for (i = 0; i < total; i++) {
            if (framebuffer[i] != 0) nonzero++;
        }
        fprintf(stderr, "INFO: Framebuffer: %u/%u non-zero pixels (%.1f%%)\n",
                nonzero, total, 100.0 * nonzero / total);
    }

    /* Write raw framebuffer to output file */
    f = fopen(output_file, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Failed to open output file '%s'\n", output_file);
        free_buffers();
        Snapshot_Free(&snap);
        return 1;
    }
    fwrite(framebuffer, 1, fb_size, f);
    fclose(f);

    /* Write PPM image if requested and palette is available */
    if (ppm_file) {
        if (snap.has_rgb_palette) {
            write_ppm(ppm_file, framebuffer,
                      snap.shared.ModeDesiredX, snap.shared.ModeDesiredY,
                      snap.rgb_palette);
        } else {
            fprintf(stderr, "WARNING: Snapshot has no RGB palette, skipping PPM output\n");
        }
    }

    /* Write game reference framebuffer if requested and available */
    if (ref_ppm_file) {
        if (snap.game_framebuffer && snap.game_fb_size > 0 && snap.has_rgb_palette) {
            write_ppm(ref_ppm_file, snap.game_framebuffer,
                      snap.shared.ModeDesiredX, snap.shared.ModeDesiredY,
                      snap.rgb_palette);
            fprintf(stderr, "INFO: Wrote game reference image (CPP-rendered): %s\n", ref_ppm_file);
        } else {
            fprintf(stderr, "WARNING: No game framebuffer in snapshot, skipping reference image\n");
        }
    }

    /* Cleanup */
    free_buffers();
    Snapshot_Free(&snap);

    return 0;
}
