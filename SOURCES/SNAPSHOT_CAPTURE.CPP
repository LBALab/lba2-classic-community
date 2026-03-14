/*
 * SNAPSHOT_CAPTURE.CPP — Scene Snapshot Capture (game-side)
 *
 * This file lives in SOURCES/ (not LIB386/) because it needs access to
 * game-level data structures like T_OBJET and ListObjet[].
 */

#include "SNAPSHOT_CAPTURE.H"
#include "C_EXTERN.H"

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
/*  HELPERS                                                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

static int write_bytes(FILE *f, const void *data, size_t size) {
    return (fwrite(data, 1, size, f) == size) ? 0 : -1;
}

static int write_section_header(FILE *f, U32 type, U32 size) {
    T_SNAPSHOT_SECTION_HEADER hdr;
    hdr.type = type;
    hdr.size = size;
    return write_bytes(f, &hdr, sizeof(hdr));
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  CAPTURE SHARED STATE                                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void capture_shared_state(T_SNAPSHOT_SHARED_STATE *s) {
    memset(s, 0, sizeof(*s));

    /* Camera */
    s->CameraX = CameraX;
    s->CameraY = CameraY;
    s->CameraZ = CameraZ;
    s->CameraAlpha = CameraAlpha;
    s->CameraBeta = CameraBeta;
    s->CameraGamma = CameraGamma;
    s->CameraXr = CameraXr;
    s->CameraYr = CameraYr;
    s->CameraZr = CameraZr;
    s->CameraZrClip = CameraZrClip;
    s->NearClip = NearClip;

    /* Projection */
    s->TypeProj = TypeProj;
    s->XCentre = XCentre;
    s->YCentre = YCentre;
    s->FRatioX = FRatioX;
    s->FRatioY = FRatioY;
    s->LFactorX = LFactorX;
    s->LFactorY = LFactorY;

    /* World matrix */
    memcpy(&s->MatriceWorld, &MatriceWorld, sizeof(TYPE_MAT));

    /* Lighting */
    s->AlphaLight = AlphaLight;
    s->BetaLight = BetaLight;
    s->GammaLight = GammaLight;
    s->NormalXLight = NormalXLight;
    s->NormalYLight = NormalYLight;
    s->NormalZLight = NormalZLight;
    s->CameraXLight = CameraXLight;
    s->CameraYLight = CameraYLight;
    s->CameraZLight = CameraZLight;
    s->LightNormalUnit = LightNormalUnit;
    s->FactorLight = FactorLight;

    /* Screen */
    s->ModeDesiredX = ModeDesiredX;
    s->ModeDesiredY = ModeDesiredY;
    s->ScreenPitch = ScreenPitch;

    /* Clipping */
    s->ClipXMin = ClipXMin;
    s->ClipYMin = ClipYMin;
    s->ClipXMax = ClipXMax;
    s->ClipYMax = ClipYMax;

    /* Z-buffer & fog */
    s->Fill_Flag_ZBuffer = Fill_Flag_ZBuffer;
    s->Fill_Flag_Fog = Fill_Flag_Fog;
    s->Fill_Flag_NZW = Fill_Flag_NZW;
    s->Fill_ZBuffer_Factor = Fill_ZBuffer_Factor;
    s->Fill_Z_Fog_Near = Fill_Z_Fog_Near;
    s->Fill_Z_Fog_Far = Fill_Z_Fog_Far;

    /* Texture mask */
    s->RepMask = RepMask;

    /* Screen dirty box */
    s->ScreenXMin = ScreenXMin;
    s->ScreenYMin = ScreenYMin;
    s->ScreenXMax = ScreenXMax;
    s->ScreenYMax = ScreenYMax;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Snapshot_CaptureScene                                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

int Snapshot_CaptureScene(const char *filename) {
    FILE *f;
    U32 n;
    T_SNAPSHOT_FILE_HEADER file_hdr;
    T_SNAPSHOT_SHARED_STATE shared;
    U32 tex_size;

    f = fopen(filename, "wb");
    if (!f) return -1;

    /* ── File header ── */
    memcpy(file_hdr.magic, SNAPSHOT_MAGIC, SNAPSHOT_MAGIC_SIZE);
    file_hdr.version = SNAPSHOT_VERSION;
    file_hdr.num_sections = 9;
    if (write_bytes(f, &file_hdr, sizeof(file_hdr)) != 0) goto fail;

    /* ── SHARED_STATE ── */
    capture_shared_state(&shared);
    if (write_section_header(f, SNAP_SECTION_SHARED_STATE, sizeof(shared)) != 0) goto fail;
    if (write_bytes(f, &shared, sizeof(shared)) != 0) goto fail;

    /* ── PALETTE ── */
    {
        U32 pal_section_size = SNAPSHOT_PALETTE_SIZE * 2;
        if (write_section_header(f, SNAP_SECTION_PALETTE, pal_section_size) != 0) goto fail;
        if (PtrTruePal) {
            if (write_bytes(f, PtrTruePal, SNAPSHOT_PALETTE_SIZE) != 0) goto fail;
        } else {
            U8 zeros[SNAPSHOT_PALETTE_SIZE];
            memset(zeros, 0, sizeof(zeros));
            if (write_bytes(f, zeros, sizeof(zeros)) != 0) goto fail;
        }
        if (write_bytes(f, Fill_Logical_Palette, SNAPSHOT_PALETTE_SIZE) != 0) goto fail;
    }

    /* ── CLUT_GOURAUD ── */
    {
        U32 clut_size = PtrCLUTGouraud ? SNAPSHOT_CLUT_SIZE : 0;
        if (write_section_header(f, SNAP_SECTION_CLUT_GOURAUD, clut_size) != 0) goto fail;
        if (clut_size > 0) {
            if (write_bytes(f, PtrCLUTGouraud, clut_size) != 0) goto fail;
        }
    }

    /* ── CLUT_FOG ── */
    {
        U32 clut_size = PtrCLUTFog ? SNAPSHOT_CLUT_SIZE : 0;
        if (write_section_header(f, SNAP_SECTION_CLUT_FOG, clut_size) != 0) goto fail;
        if (clut_size > 0) {
            if (write_bytes(f, PtrCLUTFog, clut_size) != 0) goto fail;
        }
    }

    /* ── TEXTURE_DATA ── */
    tex_size = (U32)(((RepMask & 0xFF) + 1) * (((RepMask >> 8) & 0xFF) + 1));
    {
        T_SNAPSHOT_TEXTURE_DATA_HEADER tex_hdr;
        U32 section_size;

        if (BufferTexture) {
            tex_hdr.num_textures = 1;
            section_size = sizeof(tex_hdr) + sizeof(U32) + tex_size;
        } else {
            tex_hdr.num_textures = 0;
            section_size = sizeof(tex_hdr);
        }
        if (write_section_header(f, SNAP_SECTION_TEXTURE_DATA, section_size) != 0) goto fail;
        if (write_bytes(f, &tex_hdr, sizeof(tex_hdr)) != 0) goto fail;
        if (BufferTexture) {
            if (write_bytes(f, &tex_size, sizeof(tex_size)) != 0) goto fail;
            if (write_bytes(f, BufferTexture, tex_size) != 0) goto fail;
        }
    }

    /* ── OBJECTS + BODY_DATA ── */
    {
        U32 obj_count = 0;
        T_SNAPSHOT_OBJECT snap_objects[SNAPSHOT_MAX_OBJECTS];
        U8 *body_ptrs[SNAPSHOT_MAX_BODIES];
        U32 body_sizes[SNAPSHOT_MAX_BODIES];
        U32 num_bodies = 0;

        memset(snap_objects, 0, sizeof(snap_objects));

        for (n = 0; (S32)n < NbObjets && obj_count < SNAPSHOT_MAX_OBJECTS; n++) {
            T_OBJET *pobj = &ListObjet[n];
            T_OBJ_3D *obj3d = &pobj->Obj;
            T_SNAPSHOT_OBJECT *so;
            void *body_ptr;
            U32 body_size;
            U32 body_idx;
            U32 b;

            /* Skip invisible objects */
            if (obj3d->Body.Num == -1) continue;

            /* Resolve body pointer */
            if (TransFctBody) {
                body_ptr = (void *)TransFctBody(obj3d->Body.Num);
            } else {
                body_ptr = obj3d->Body.Ptr;
            }
            if (!body_ptr) continue;

            body_size = Snapshot_ComputeBodySize(body_ptr);

            /* Deduplicate body data */
            body_idx = num_bodies;
            for (b = 0; b < num_bodies; b++) {
                if (body_ptrs[b] == (U8 *)body_ptr) {
                    body_idx = b;
                    break;
                }
            }
            if (body_idx == num_bodies && num_bodies < SNAPSHOT_MAX_BODIES) {
                body_ptrs[num_bodies] = (U8 *)body_ptr;
                body_sizes[num_bodies] = body_size;
                num_bodies++;
            }

            so = &snap_objects[obj_count];
            so->X = obj3d->X;
            so->Y = obj3d->Y;
            so->Z = obj3d->Z;
            so->Alpha = obj3d->Alpha;
            so->Beta = obj3d->Beta;
            so->Gamma = obj3d->Gamma;
            so->render_type = 1; /* ObjectDisplay */
            so->body_index = body_idx;
            so->body_data_size = body_size;
            so->texture_index = 0;
            so->texture_data_size = tex_size;
            so->has_animation = 1;

            /* Copy T_OBJ_3D animation fields */
            so->LastFrame = obj3d->LastFrame;
            so->NextFrame = obj3d->NextFrame;
            so->LoopFrame = obj3d->LoopFrame;
            so->LastTimer = obj3d->LastTimer;
            so->NextTimer = obj3d->NextTimer;
            so->NbFrames = obj3d->NbFrames;
            so->NbGroups = obj3d->NbGroups;
            so->LastNbGroups = obj3d->LastNbGroups;
            so->NextNbGroups = obj3d->NextNbGroups;
            so->Interpolator = obj3d->Interpolator;
            so->Time = obj3d->Time;
            so->Status = obj3d->Status;
            so->Master = obj3d->Master;
            so->LastAnimStepX = obj3d->LastAnimStepX;
            so->LastAnimStepY = obj3d->LastAnimStepY;
            so->LastAnimStepZ = obj3d->LastAnimStepZ;
            so->LastAnimStepAlpha = obj3d->LastAnimStepAlpha;
            so->LastAnimStepBeta = obj3d->LastAnimStepBeta;
            so->LastAnimStepGamma = obj3d->LastAnimStepGamma;
            so->LastOfsIsPtr = obj3d->LastOfsIsPtr;

            /* Per-group animation state */
            {
                U32 g;
                for (g = 0; g < 30; g++) {
                    so->CurrentFrame_Type[g] = obj3d->CurrentFrame[g].Type;
                    so->CurrentFrame_Alpha[g] = obj3d->CurrentFrame[g].Alpha;
                    so->CurrentFrame_Beta[g] = obj3d->CurrentFrame[g].Beta;
                    so->CurrentFrame_Gamma[g] = obj3d->CurrentFrame[g].Gamma;
                }
            }

            /* Animation frame data (Phase 2 enhancement) */
            so->last_frame_data_offset = 0;
            so->last_frame_data_size = 0;
            so->next_frame_data_offset = 0;
            so->next_frame_data_size = 0;

            obj_count++;
        }

        /* Write OBJECTS section */
        {
            T_SNAPSHOT_OBJECTS_HEADER obj_hdr;
            U32 section_size;
            obj_hdr.num_objects = obj_count;
            section_size = sizeof(obj_hdr) + obj_count * sizeof(T_SNAPSHOT_OBJECT);
            if (write_section_header(f, SNAP_SECTION_OBJECTS, section_size) != 0) goto fail;
            if (write_bytes(f, &obj_hdr, sizeof(obj_hdr)) != 0) goto fail;
            for (n = 0; n < obj_count; n++) {
                if (write_bytes(f, &snap_objects[n], sizeof(T_SNAPSHOT_OBJECT)) != 0) goto fail;
            }
        }

        /* Write BODY_DATA section */
        {
            T_SNAPSHOT_BODY_DATA_HEADER body_hdr;
            U32 section_size;
            body_hdr.num_bodies = num_bodies;
            section_size = sizeof(body_hdr);
            for (n = 0; n < num_bodies; n++) {
                section_size += sizeof(U32) + body_sizes[n];
            }
            if (write_section_header(f, SNAP_SECTION_BODY_DATA, section_size) != 0) goto fail;
            if (write_bytes(f, &body_hdr, sizeof(body_hdr)) != 0) goto fail;
            for (n = 0; n < num_bodies; n++) {
                if (write_bytes(f, &body_sizes[n], sizeof(U32)) != 0) goto fail;
                if (write_bytes(f, body_ptrs[n], body_sizes[n]) != 0) goto fail;
            }
        }
    }

    /* ── TABOFFLINE ── */
    {
        T_SNAPSHOT_TABOFFLINE_HEADER tol_hdr;
        U32 count = ModeDesiredY;
        U32 section_size = sizeof(tol_hdr) + count * sizeof(U32);
        tol_hdr.count = count;
        if (write_section_header(f, SNAP_SECTION_TABOFFLINE, section_size) != 0) goto fail;
        if (write_bytes(f, &tol_hdr, sizeof(tol_hdr)) != 0) goto fail;
        if (write_bytes(f, TabOffLine, count * sizeof(U32)) != 0) goto fail;
    }

    /* ── END ── */
    if (write_section_header(f, SNAP_SECTION_END, 0) != 0) goto fail;

    fclose(f);
    return 0;

fail:
    fclose(f);
    return -1;
}
