/*
 * SNAPSHOT.CPP — Scene Snapshot Capture & Load
 *
 * Captures the complete rendering state during gameplay into a binary file,
 * and loads it back for ASM-vs-CPP equivalence testing.
 */

#include <SNAPSHOT/SNAPSHOT.H>
#include <3D/CAMERA.H>
#include <3D/LIGHT.H>
#include <3D/DATAMAT.H>
#include <POLYGON/POLY.H>
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

static int read_bytes(FILE *f, void *data, size_t size) {
    return (fread(data, 1, size, f) == size) ? 0 : -1;
}

static int write_section_header(FILE *f, U32 type, U32 size) {
    T_SNAPSHOT_SECTION_HEADER hdr;
    hdr.type = type;
    hdr.size = size;
    return write_bytes(f, &hdr, sizeof(hdr));
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  BODY SIZE COMPUTATION                                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

U32 Snapshot_ComputeBodySize(const void *body) {
    const T_BODY_HEADER *bh = (const T_BODY_HEADER *)body;
    U32 max_end = (U32)sizeof(T_BODY_HEADER);

    /* Walk each sub-array and find the maximum extent */

    /* Points: TYPE_VT16 = 8 bytes each */
    if (bh->NbPoints > 0 && bh->OffPoints > 0) {
        U32 end = (U32)bh->OffPoints + (U32)bh->NbPoints * 8;
        if (end > max_end) max_end = end;
    }

    /* Normals: TYPE_VT16 = 8 bytes each */
    if (bh->NbNormales > 0 && bh->OffNormales > 0) {
        U32 end = (U32)bh->OffNormales + (U32)bh->NbNormales * 8;
        if (end > max_end) max_end = end;
    }

    /* Face normals: TYPE_VT16 = 8 bytes each */
    if (bh->NbNormFaces > 0 && bh->OffNormFaces > 0) {
        U32 end = (U32)bh->OffNormFaces + (U32)bh->NbNormFaces * 8;
        if (end > max_end) max_end = end;
    }

    /* Groups: 16 bytes each (TYPE_VT16 + 4 extra S16) */
    if (bh->NbGroupes > 0 && bh->OffGroupes > 0) {
        U32 end = (U32)bh->OffGroupes + (U32)bh->NbGroupes * 16;
        if (end > max_end) max_end = end;
    }

    /* Lines: estimate 8 bytes each (2 point indices + color) */
    if (bh->NbLines > 0 && bh->OffLines > 0) {
        U32 end = (U32)bh->OffLines + (U32)bh->NbLines * 8;
        if (end > max_end) max_end = end;
    }

    /* Spheres: estimate 16 bytes each */
    if (bh->NbSpheres > 0 && bh->OffSpheres > 0) {
        U32 end = (U32)bh->OffSpheres + (U32)bh->NbSpheres * 16;
        if (end > max_end) max_end = end;
    }

    /* Textures data after OffTextures */
    if (bh->NbTextures > 0 && bh->OffTextures > 0) {
        /* Each texture entry is variable-size; approximate by taking
         * the offset of the next block or just add a generous estimate.
         * For safety, use SizeHeader if available. */
        U32 end = (U32)bh->OffTextures + (U32)bh->NbTextures * 16;
        if (end > max_end) max_end = end;
    }

    /* Polygons: walk the polygon block.
     * Polygon data starts at OffPolys. Each T_POLY_HEADER is followed by
     * variable-size polygon data. The block ends at OffLines (or end of body).
     * For simplicity, use OffLines as the end of polygon data. */
    if (bh->NbPolys > 0 && bh->OffPolys > 0) {
        U32 poly_end;
        if (bh->OffLines > 0) {
            poly_end = (U32)bh->OffLines;
        } else if (bh->OffSpheres > 0) {
            poly_end = (U32)bh->OffSpheres;
        } else if (bh->OffTextures > 0) {
            poly_end = (U32)bh->OffTextures;
        } else {
            /* Worst case: use header size * generous factor */
            poly_end = (U32)bh->OffPolys + (U32)bh->NbPolys * 32;
        }
        if (poly_end > max_end) max_end = poly_end;
    }

    /* Use SizeHeader if it's larger (the header may know the total size) */
    if (bh->SizeHeader > 0 && (U32)bh->SizeHeader > max_end) {
        max_end = (U32)bh->SizeHeader;
    }

    return max_end;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  LOAD — reads a snapshot file into memory                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

int Snapshot_Load(const char *filename, T_SCENE_SNAPSHOT *out) {
    FILE *f;
    T_SNAPSHOT_FILE_HEADER file_hdr;
    U32 i;
    int done = 0;

    memset(out, 0, sizeof(T_SCENE_SNAPSHOT));

    f = fopen(filename, "rb");
    if (!f) return -1;

    /* Read file header */
    if (read_bytes(f, &file_hdr, sizeof(file_hdr)) != 0) goto fail;
    if (memcmp(file_hdr.magic, SNAPSHOT_MAGIC, SNAPSHOT_MAGIC_SIZE) != 0) goto fail;
    if (file_hdr.version != SNAPSHOT_VERSION) goto fail;

    /* Read sections */
    for (i = 0; !done; i++) {
        T_SNAPSHOT_SECTION_HEADER sec_hdr;
        if (read_bytes(f, &sec_hdr, sizeof(sec_hdr)) != 0) goto fail;

        switch (sec_hdr.type) {

        case SNAP_SECTION_SHARED_STATE:
            if (sec_hdr.size != sizeof(T_SNAPSHOT_SHARED_STATE)) goto fail;
            if (read_bytes(f, &out->shared, sizeof(out->shared)) != 0) goto fail;
            break;

        case SNAP_SECTION_PALETTE:
            if (sec_hdr.size < SNAPSHOT_PALETTE_SIZE * 2) goto fail;
            if (read_bytes(f, out->palette, SNAPSHOT_PALETTE_SIZE) != 0) goto fail;
            if (read_bytes(f, out->logical_palette, SNAPSHOT_PALETTE_SIZE) != 0) goto fail;
            break;

        case SNAP_SECTION_RGB_PALETTE:
            if (sec_hdr.size >= SNAPSHOT_RGB_PALETTE_SIZE) {
                if (read_bytes(f, out->rgb_palette, SNAPSHOT_RGB_PALETTE_SIZE) != 0) goto fail;
                out->has_rgb_palette = 1;
            }
            break;

        case SNAP_SECTION_CLUT_GOURAUD:
            if (sec_hdr.size > 0) {
                out->clut_gouraud = (U8 *)malloc(sec_hdr.size);
                if (!out->clut_gouraud) goto fail;
                if (read_bytes(f, out->clut_gouraud, sec_hdr.size) != 0) goto fail;
            }
            break;

        case SNAP_SECTION_CLUT_FOG:
            if (sec_hdr.size > 0) {
                out->clut_fog = (U8 *)malloc(sec_hdr.size);
                if (!out->clut_fog) goto fail;
                if (read_bytes(f, out->clut_fog, sec_hdr.size) != 0) goto fail;
            }
            break;

        case SNAP_SECTION_OBJECTS: {
            T_SNAPSHOT_OBJECTS_HEADER obj_hdr;
            U32 j;
            if (read_bytes(f, &obj_hdr, sizeof(obj_hdr)) != 0) goto fail;
            if (obj_hdr.num_objects > SNAPSHOT_MAX_OBJECTS) goto fail;
            out->num_objects = obj_hdr.num_objects;
            for (j = 0; j < obj_hdr.num_objects; j++) {
                if (read_bytes(f, &out->objects[j], sizeof(T_SNAPSHOT_OBJECT)) != 0) goto fail;
            }
            break;
        }

        case SNAP_SECTION_BODY_DATA: {
            T_SNAPSHOT_BODY_DATA_HEADER body_hdr;
            U32 j;
            if (read_bytes(f, &body_hdr, sizeof(body_hdr)) != 0) goto fail;
            if (body_hdr.num_bodies > SNAPSHOT_MAX_BODIES) goto fail;
            out->num_bodies = body_hdr.num_bodies;
            for (j = 0; j < body_hdr.num_bodies; j++) {
                U32 bsize;
                if (read_bytes(f, &bsize, sizeof(bsize)) != 0) goto fail;
                out->body_sizes[j] = bsize;
                out->body_data[j] = (U8 *)malloc(bsize);
                if (!out->body_data[j]) goto fail;
                if (read_bytes(f, out->body_data[j], bsize) != 0) goto fail;
            }
            break;
        }

        case SNAP_SECTION_TEXTURE_DATA: {
            T_SNAPSHOT_TEXTURE_DATA_HEADER tex_hdr;
            U32 j;
            if (read_bytes(f, &tex_hdr, sizeof(tex_hdr)) != 0) goto fail;
            if (tex_hdr.num_textures > SNAPSHOT_MAX_TEXTURES) goto fail;
            out->num_textures = tex_hdr.num_textures;
            for (j = 0; j < tex_hdr.num_textures; j++) {
                U32 tsize;
                if (read_bytes(f, &tsize, sizeof(tsize)) != 0) goto fail;
                out->texture_sizes[j] = tsize;
                out->texture_data[j] = (U8 *)malloc(tsize);
                if (!out->texture_data[j]) goto fail;
                if (read_bytes(f, out->texture_data[j], tsize) != 0) goto fail;
            }
            break;
        }

        case SNAP_SECTION_TABOFFLINE: {
            T_SNAPSHOT_TABOFFLINE_HEADER tol_hdr;
            if (read_bytes(f, &tol_hdr, sizeof(tol_hdr)) != 0) goto fail;
            out->taboffline_count = tol_hdr.count;
            out->taboffline = (U32 *)malloc(tol_hdr.count * sizeof(U32));
            if (!out->taboffline) goto fail;
            if (read_bytes(f, out->taboffline, tol_hdr.count * sizeof(U32)) != 0) goto fail;
            break;
        }

        case SNAP_SECTION_SORT_ORDER: {
            T_SNAPSHOT_SORT_ORDER_HEADER sort_hdr;
            if (read_bytes(f, &sort_hdr, sizeof(sort_hdr)) != 0) goto fail;
            out->sort_order_count = sort_hdr.num_entries;
            out->sort_order = (U32 *)malloc(sort_hdr.num_entries * sizeof(U32));
            if (!out->sort_order) goto fail;
            if (read_bytes(f, out->sort_order, sort_hdr.num_entries * sizeof(U32)) != 0) goto fail;
            break;
        }

        case SNAP_SECTION_END:
            done = 1;
            break;

        default:
            /* Unknown section: skip it */
            if (fseek(f, (long)sec_hdr.size, SEEK_CUR) != 0) goto fail;
            break;
        }
    }

    fclose(f);
    return 0;

fail:
    fclose(f);
    Snapshot_Free(out);
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  APPLY STATE — sets all rendering globals from a loaded snapshot           */
/* ═══════════════════════════════════════════════════════════════════════════ */

void Snapshot_ApplyState(const T_SCENE_SNAPSHOT *snap) {
    const T_SNAPSHOT_SHARED_STATE *s = &snap->shared;

    /* Camera */
    CameraX = s->CameraX;
    CameraY = s->CameraY;
    CameraZ = s->CameraZ;
    CameraAlpha = s->CameraAlpha;
    CameraBeta = s->CameraBeta;
    CameraGamma = s->CameraGamma;
    CameraXr = s->CameraXr;
    CameraYr = s->CameraYr;
    CameraZr = s->CameraZr;
    CameraZrClip = s->CameraZrClip;
    NearClip = s->NearClip;

    /* Projection */
    TypeProj = s->TypeProj;
    XCentre = s->XCentre;
    YCentre = s->YCentre;
    FRatioX = s->FRatioX;
    FRatioY = s->FRatioY;
    LFactorX = s->LFactorX;
    LFactorY = s->LFactorY;

    /* World matrix */
    memcpy(&MatriceWorld, &s->MatriceWorld, sizeof(TYPE_MAT));

    /* Lighting */
    AlphaLight = s->AlphaLight;
    BetaLight = s->BetaLight;
    GammaLight = s->GammaLight;
    NormalXLight = s->NormalXLight;
    NormalYLight = s->NormalYLight;
    NormalZLight = s->NormalZLight;
    CameraXLight = s->CameraXLight;
    CameraYLight = s->CameraYLight;
    CameraZLight = s->CameraZLight;
    LightNormalUnit = s->LightNormalUnit;
    FactorLight = s->FactorLight;

    /* Screen */
    ModeDesiredX = s->ModeDesiredX;
    ModeDesiredY = s->ModeDesiredY;
    ScreenPitch = s->ScreenPitch;

    /* Clipping */
    ClipXMin = s->ClipXMin;
    ClipYMin = s->ClipYMin;
    ClipXMax = s->ClipXMax;
    ClipYMax = s->ClipYMax;

    /* Z-buffer & fog */
    Fill_Flag_ZBuffer = s->Fill_Flag_ZBuffer;
    Fill_Flag_Fog = s->Fill_Flag_Fog;
    Fill_Flag_NZW = s->Fill_Flag_NZW;
    Fill_ZBuffer_Factor = s->Fill_ZBuffer_Factor;
    Fill_Z_Fog_Near = s->Fill_Z_Fog_Near;
    Fill_Z_Fog_Far = s->Fill_Z_Fog_Far;

    /* Texture */
    RepMask = s->RepMask;

    /* ScreenXY */
    ScreenXMin = s->ScreenXMin;
    ScreenYMin = s->ScreenYMin;
    ScreenXMax = s->ScreenXMax;
    ScreenYMax = s->ScreenYMax;

    /* Palette */
    if (PtrTruePal) {
        memcpy(PtrTruePal, snap->palette, SNAPSHOT_PALETTE_SIZE);
    }
    memcpy(Fill_Logical_Palette, snap->logical_palette, SNAPSHOT_PALETTE_SIZE);

    /* CLUTs */
    if (snap->clut_gouraud && PtrCLUTGouraud) {
        memcpy(PtrCLUTGouraud, snap->clut_gouraud, SNAPSHOT_CLUT_SIZE);
    }
    if (snap->clut_fog && PtrCLUTFog) {
        memcpy(PtrCLUTFog, snap->clut_fog, SNAPSHOT_CLUT_SIZE);
    }

    /* TabOffLine */
    if (snap->taboffline) {
        U32 count = snap->taboffline_count;
        if (count > 1024) count = 1024; /* ADELINE_MAX_Y_RES */
        memcpy(TabOffLine, snap->taboffline, count * sizeof(U32));
        PTR_TabOffLine = TabOffLine;
    }

    /* Texture page */
    if (snap->num_textures > 0 && snap->texture_data[0]) {
        PtrMap = snap->texture_data[0];
        ObjPtrMap = snap->texture_data[0];
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  FREE — release all dynamically allocated memory                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

void Snapshot_Free(T_SCENE_SNAPSHOT *snap) {
    U32 i;

    if (snap->clut_gouraud) { free(snap->clut_gouraud); snap->clut_gouraud = NULL; }
    if (snap->clut_fog)     { free(snap->clut_fog);     snap->clut_fog = NULL; }

    for (i = 0; i < snap->num_bodies; i++) {
        if (snap->body_data[i]) { free(snap->body_data[i]); snap->body_data[i] = NULL; }
    }

    for (i = 0; i < snap->num_textures; i++) {
        if (snap->texture_data[i]) { free(snap->texture_data[i]); snap->texture_data[i] = NULL; }
    }

    if (snap->taboffline) { free(snap->taboffline); snap->taboffline = NULL; }
    if (snap->sort_order) { free(snap->sort_order); snap->sort_order = NULL; }

    for (i = 0; i < snap->num_anim_blobs; i++) {
        if (snap->anim_blob_data[i]) { free(snap->anim_blob_data[i]); snap->anim_blob_data[i] = NULL; }
    }

    memset(snap, 0, sizeof(T_SCENE_SNAPSHOT));
}
