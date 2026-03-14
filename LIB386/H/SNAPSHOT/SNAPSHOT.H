/*
 * SNAPSHOT.H — Scene Snapshot Format for ASM-vs-CPP Equivalence Testing
 *
 * Captures the complete rendering state during gameplay so it can be replayed
 * through both the ASM and CPP pipelines independently to verify byte-for-byte
 * identical framebuffer output.
 *
 * File format: .lba2snap (structured binary)
 */

#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <3D/DATAMAT.H>

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  FILE FORMAT CONSTANTS                                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

#ifdef __cplusplus
extern "C" {
#endif

#define SNAPSHOT_MAGIC      "LBA2SNAP"   /* 8 bytes */
#define SNAPSHOT_MAGIC_SIZE 8
#define SNAPSHOT_VERSION    1

/* Section type identifiers (4-byte tags) */
#define SNAP_SECTION_SHARED_STATE   0x53485244  /* "SHRD" */
#define SNAP_SECTION_PALETTE        0x50414C54  /* "PALT" */
#define SNAP_SECTION_CLUT_GOURAUD   0x434C5447  /* "CLTG" */
#define SNAP_SECTION_CLUT_FOG       0x434C5446  /* "CLTF" */
#define SNAP_SECTION_OBJECTS        0x4F424A53  /* "OBJS" */
#define SNAP_SECTION_BODY_DATA      0x424F4459  /* "BODY" */
#define SNAP_SECTION_TEXTURE_DATA   0x54455854  /* "TEXT" */
#define SNAP_SECTION_TABOFFLINE     0x544F4C4E  /* "TOLN" */
#define SNAP_SECTION_SORT_ORDER     0x534F5254  /* "SORT" */
#define SNAP_SECTION_RGB_PALETTE    0x52474250  /* "RGBP" */
#define SNAP_SECTION_END            0x454E4421  /* "END!" */

#define SNAPSHOT_RGB_PALETTE_SIZE 768  /* 256 entries × 3 bytes (R,G,B) */

#define SNAPSHOT_MAX_OBJECTS    200
#define SNAPSHOT_MAX_BODIES     100
#define SNAPSHOT_MAX_TEXTURES   50
#define SNAPSHOT_CLUT_SIZE      65536  /* 256 * 256 bytes */
#define SNAPSHOT_PALETTE_SIZE   256

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  FILE HEADER                                                               */
/* ═══════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)

typedef struct {
    U8  magic[SNAPSHOT_MAGIC_SIZE];  /* "LBA2SNAP" */
    U32 version;                     /* SNAPSHOT_VERSION */
    U32 num_sections;                /* Number of sections in file */
} T_SNAPSHOT_FILE_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SECTION HEADER — precedes every section                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 type;       /* SNAP_SECTION_* constant */
    U32 size;       /* Size in bytes of section data (after this header) */
} T_SNAPSHOT_SECTION_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SHARED_STATE — Camera, projection, lighting, screen, clipping, etc.       */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Camera position & angles */
    S32 CameraX, CameraY, CameraZ;
    S32 CameraAlpha, CameraBeta, CameraGamma;
    S32 CameraXr, CameraYr, CameraZr;
    S32 CameraZrClip;
    S32 NearClip;

    /* Projection */
    S32 TypeProj;
    S32 XCentre, YCentre;
    float FRatioX, FRatioY;
    S32 LFactorX, LFactorY;

    /* World matrix (48 bytes as 12 floats) */
    TYPE_MAT MatriceWorld;

    /* Lighting */
    S32 AlphaLight, BetaLight, GammaLight;
    S32 NormalXLight, NormalYLight, NormalZLight;
    S32 CameraXLight, CameraYLight, CameraZLight;
    S32 LightNormalUnit;
    float FactorLight;

    /* Screen dimensions */
    U32 ModeDesiredX, ModeDesiredY;
    U32 ScreenPitch;

    /* Clipping region */
    S32 ClipXMin, ClipYMin, ClipXMax, ClipYMax;

    /* Z-buffer & fog flags */
    U8  Fill_Flag_ZBuffer;
    U8  Fill_Flag_Fog;
    U8  Fill_Flag_NZW;
    U8  _pad1;
    U32 Fill_ZBuffer_Factor;
    S32 Fill_Z_Fog_Near;
    S32 Fill_Z_Fog_Far;

    /* Texture state */
    S32 RepMask;

    /* ScreenXY (dirty box — reset before rendering) */
    S32 ScreenXMin, ScreenYMin, ScreenXMax, ScreenYMax;
} T_SNAPSHOT_SHARED_STATE;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  OBJECT — One per visible object in the scene                              */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* World position & rotation */
    S32 X, Y, Z;
    S32 Alpha, Beta, Gamma;

    /* Which rendering entry point to use */
    U8  render_type;     /* 0 = BodyDisplay, 1 = ObjectDisplay */
    U8  _pad[3];

    /* Body data reference (index into BODY_DATA section) */
    U32 body_index;
    U32 body_data_size;  /* Size of T_BODY_HEADER + all sub-data */

    /* Texture data reference (index into TEXTURE_DATA section) */
    U32 texture_index;
    U32 texture_data_size;

    /* ObjectDisplay animation state (only if render_type == 1) */
    U8  has_animation;
    U8  _pad2[3];

    /* T_OBJ_3D fields needed for ObjectDisplay replay */
    S32 LastFrame;
    S32 NextFrame;
    S32 LoopFrame;
    U32 LastTimer;
    U32 NextTimer;
    U32 NbFrames;
    U32 NbGroups;
    U32 LastNbGroups;
    U32 NextNbGroups;
    U32 Interpolator;
    U32 Time;
    U32 Status;
    U32 Master;
    S32 LastAnimStepX, LastAnimStepY, LastAnimStepZ;
    S32 LastAnimStepAlpha, LastAnimStepBeta, LastAnimStepGamma;
    U32 LastOfsIsPtr;

    /* Per-group animation state */
    S16 CurrentFrame_Type[30];
    S16 CurrentFrame_Alpha[30];
    S16 CurrentFrame_Beta[30];
    S16 CurrentFrame_Gamma[30];

    /* Animation frame data blob offsets (into BODY_DATA section)
     * These point to the keyframe data needed by the interpolator */
    U32 last_frame_data_offset;
    U32 last_frame_data_size;
    U32 next_frame_data_offset;
    U32 next_frame_data_size;
} T_SNAPSHOT_OBJECT;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  OBJECTS section header                                                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 num_objects;
    /* Followed by num_objects × T_SNAPSHOT_OBJECT */
} T_SNAPSHOT_OBJECTS_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  BODY_DATA section — deduplicated body blobs                               */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 num_bodies;
    /* Followed by num_bodies × { U32 body_size, U8 data[body_size] } */
} T_SNAPSHOT_BODY_DATA_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  TEXTURE_DATA section — deduplicated texture pages                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 num_textures;
    /* Followed by num_textures × { U32 tex_size, U8 data[tex_size] } */
} T_SNAPSHOT_TEXTURE_DATA_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  TABOFFLINE section                                                        */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 count;   /* Number of entries (= ModeDesiredY) */
    /* Followed by count × U32 offsets */
} T_SNAPSHOT_TABOFFLINE_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SORT_ORDER section — object render order                                  */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 num_entries;
    /* Followed by num_entries × U32 object indices (in render order) */
} T_SNAPSHOT_SORT_ORDER_HEADER;

#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  IN-MEMORY SNAPSHOT REPRESENTATION                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    T_SNAPSHOT_SHARED_STATE shared;

    /* Palette */
    U8  palette[SNAPSHOT_PALETTE_SIZE];
    U8  logical_palette[SNAPSHOT_PALETTE_SIZE];
    U8  rgb_palette[SNAPSHOT_RGB_PALETTE_SIZE];  /* 256 RGB triplets */
    U8  has_rgb_palette;                          /* 1 if rgb_palette is valid */

    /* CLUTs (allocated dynamically) */
    U8  *clut_gouraud;    /* SNAPSHOT_CLUT_SIZE bytes if non-NULL */
    U8  *clut_fog;        /* SNAPSHOT_CLUT_SIZE bytes if non-NULL */

    /* Objects */
    U32 num_objects;
    T_SNAPSHOT_OBJECT objects[SNAPSHOT_MAX_OBJECTS];

    /* Body data (allocated dynamically) */
    U32 num_bodies;
    U32 body_sizes[SNAPSHOT_MAX_BODIES];
    U8  *body_data[SNAPSHOT_MAX_BODIES];  /* Raw T_BODY_HEADER + sub-data */

    /* Texture data (allocated dynamically) */
    U32 num_textures;
    U32 texture_sizes[SNAPSHOT_MAX_TEXTURES];
    U8  *texture_data[SNAPSHOT_MAX_TEXTURES]; /* Raw texture page */

    /* TabOffLine */
    U32 taboffline_count;
    U32 *taboffline;     /* Allocated: taboffline_count × U32 */

    /* Sort order */
    U32 sort_order_count;
    U32 *sort_order;     /* Allocated: sort_order_count × U32 */

    /* Animation frame data (allocated alongside body data) */
    /* Each object may reference frame data blobs stored here */
    U32 num_anim_blobs;
    U32 anim_blob_sizes[SNAPSHOT_MAX_OBJECTS * 2]; /* last + next per object */
    U8  *anim_blob_data[SNAPSHOT_MAX_OBJECTS * 2];
} T_SCENE_SNAPSHOT;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  PUBLIC API                                                                */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Capture the current scene rendering state and write it to a file.
 * Call this at the start of AffScene() after camera/lights are set up.
 * @param filename  Output file path (.lba2snap)
 * @return 0 on success, -1 on failure
 */
int Snapshot_CaptureScene(const char *filename);

/**
 * Load a snapshot file into memory.
 * @param filename  Input file path (.lba2snap)
 * @param out       Output structure (caller must free with Snapshot_Free)
 * @return 0 on success, -1 on failure
 */
int Snapshot_Load(const char *filename, T_SCENE_SNAPSHOT *out);

/**
 * Apply the loaded snapshot state to all rendering globals.
 * Sets camera, lights, projection, palette, CLUTs, etc.
 * @param snap  Loaded snapshot
 */
void Snapshot_ApplyState(const T_SCENE_SNAPSHOT *snap);

/**
 * Free all dynamically allocated memory in a snapshot.
 * @param snap  Snapshot to free
 */
void Snapshot_Free(T_SCENE_SNAPSHOT *snap);

/**
 * Compute body blob size from T_BODY_HEADER.
 * Walks all sub-arrays (points, normals, polygons, lines, spheres, textures)
 * and returns the total byte size of the body data block.
 * @param body  Pointer to body header in memory
 * @return Total size in bytes
 */
U32 Snapshot_ComputeBodySize(const void *body);

#ifdef __cplusplus
}
#endif
