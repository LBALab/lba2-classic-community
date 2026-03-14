/*
 * POLY_RECORDING.H — Polygon Call Recording Format for ASM-vs-CPP Equivalence Testing
 *
 * Records the actual polygon rendering calls made during a game frame
 * (Fill_Poly, Fill_Sphere, Line_A) plus rendering state changes
 * (Switch_Fillers, SetFog, SetCLUT, texture changes), and replays them
 * through both ASM and CPP pipelines for byte-for-byte comparison.
 *
 * File format: .lba2polyrec (structured binary)
 *
 * This approach captures both 3D object rendering AND exterior terrain
 * (anything that goes through Fill_Poly), without needing to capture
 * scene graph, bodies, animations, or projection matrices.
 */

#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  FILE FORMAT CONSTANTS                                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

#define POLYREC_MAGIC       "LBA2PREC"   /* 8 bytes */
#define POLYREC_MAGIC_SIZE  8
#define POLYREC_VERSION     1

/* Section type identifiers (4-byte tags) */
#define POLYREC_SECTION_CLUT        0x434C5554  /* "CLUT" */
#define POLYREC_SECTION_TEXTURES    0x54455854  /* "TEXT" */
#define POLYREC_SECTION_INIT_STATE  0x494E4954  /* "INIT" */
#define POLYREC_SECTION_EVENTS      0x45564E54  /* "EVNT" */
#define POLYREC_SECTION_REF_FB      0x52454646  /* "REFF" — game's own framebuffer for validation */
#define POLYREC_SECTION_END         0x454E4421  /* "END!" */

#define POLYREC_MAX_TEXTURES    64
#define POLYREC_CLUT_SIZE       65536   /* 256 * 256 bytes */
#define POLYREC_PALETTE_SIZE    256
#define POLYREC_RGB_PALETTE_SIZE 768    /* 256 entries × 3 bytes (R,G,B) */

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  EVENT TYPES                                                               */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* State change events */
#define POLYREC_EVT_SWITCH_FILLERS  0x02
#define POLYREC_EVT_SET_FOG         0x03
#define POLYREC_EVT_SET_CLUT        0x04
#define POLYREC_EVT_SET_TEXTURE     0x05
#define POLYREC_EVT_SET_CLIP        0x06
#define POLYREC_EVT_SET_REPMASK     0x07
#define POLYREC_EVT_SET_FILL_PATCH  0x08

/* Draw call events */
#define POLYREC_EVT_FILL_POLY       0x10
#define POLYREC_EVT_FILL_SPHERE     0x11
#define POLYREC_EVT_LINE_A          0x12

/* Sentinel */
#define POLYREC_EVT_EOF             0xFF

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  FILE HEADER                                                               */
/* ═══════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)

typedef struct {
    U8  magic[POLYREC_MAGIC_SIZE];  /* "LBA2PREC" */
    U32 version;                     /* POLYREC_VERSION */
    U16 screen_width;
    U16 screen_height;
    U32 screen_pitch;
    U32 num_events;                  /* total event count */
    S32 clip_xmin;
    S32 clip_ymin;
    S32 clip_xmax;
    S32 clip_ymax;
} T_POLYREC_FILE_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  SECTION HEADER — precedes every section                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 type;       /* POLYREC_SECTION_* constant */
    U32 size;       /* Size in bytes of section data (after this header) */
} T_POLYREC_SECTION_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  CLUT SECTION                                                              */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U8  palette[POLYREC_PALETTE_SIZE];       /* Fill_Logical_Palette initial state */
    U8  rgb_palette[POLYREC_RGB_PALETTE_SIZE]; /* RGB palette for PPM output */
    U8  has_rgb_palette;                     /* 1 if rgb_palette is valid */
    U8  _pad[3];
    U32 initial_clut_line;                   /* initial SetCLUT line offset */
    /* Followed by:
     *   U8 clut_fog[POLYREC_CLUT_SIZE]      — PtrCLUTFog base table
     *   U8 clut_gouraud[POLYREC_CLUT_SIZE]  — PtrCLUTGouraud base table
     */
} T_POLYREC_CLUT_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  TEXTURE TABLE SECTION                                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 texture_id;     /* sequential ID (0, 1, 2, ...) */
    U32 rep_mask;       /* RepMask value when this texture is active */
    U32 data_size;      /* raw texture bytes */
    /* Followed by: U8 data[data_size] */
} T_POLYREC_TEXTURE_ENTRY;

typedef struct {
    U32 num_textures;
    /* Followed by num_textures x { T_POLYREC_TEXTURE_ENTRY + data } */
} T_POLYREC_TEXTURES_HEADER;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  INITIAL STATE SECTION                                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 filler_bank;       /* current Switch_Fillers bank */
    S32 fog_near;
    S32 fog_far;
    U32 fill_patch;        /* Fill_Patch value */
    U32 active_texture;    /* index into texture table (0xFFFFFFFF = none) */
    U32 rep_mask;          /* current RepMask */
    S32 clip_xmin;
    S32 clip_ymin;
    S32 clip_xmax;
    S32 clip_ymax;
} T_POLYREC_INIT_STATE;

#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  IN-MEMORY TEXTURE ENTRY (for recording and replay)                        */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    U32 texture_id;
    U32 rep_mask;
    U32 data_size;
    U8  *data;             /* Pointer to texture pixel data */
    U8  *original_ptr;     /* Original PtrMap value (for deduplication) */
} T_POLYREC_MEM_TEXTURE;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  IN-MEMORY RECORDING STATE                                                 */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Header data */
    U16 screen_width;
    U16 screen_height;
    U32 screen_pitch;

    /* CLUT data */
    U8  palette[POLYREC_PALETTE_SIZE];
    U8  rgb_palette[POLYREC_RGB_PALETTE_SIZE];
    U8  has_rgb_palette;
    U32 initial_clut_line;
    U8  *clut_fog;           /* POLYREC_CLUT_SIZE bytes if non-NULL */
    U8  *clut_gouraud;       /* POLYREC_CLUT_SIZE bytes if non-NULL */

    /* Texture table */
    U32 num_textures;
    T_POLYREC_MEM_TEXTURE textures[POLYREC_MAX_TEXTURES];

    /* Initial state */
    T_POLYREC_INIT_STATE init_state;

    /* Event buffer (variable-length events serialized sequentially) */
    U8  *event_buf;
    U32  event_buf_size;     /* Total allocated capacity */
    U32  event_buf_used;     /* Bytes written so far */
    U32  num_events;         /* Event count */

    /* State tracking for delta compression */
    U32  last_filler_bank;
    S32  last_fog_near;
    S32  last_fog_far;
    U32  last_clut_line;
    U8  *last_ptr_map;       /* Last PtrMap pointer (for texture change detection) */
    S32  last_rep_mask;
    U32  last_fill_patch;
    S32  last_clip_xmin;
    S32  last_clip_ymin;
    S32  last_clip_xmax;
    S32  last_clip_ymax;

    /* Reference framebuffer (game's actual output after rendering) */
    U32 ref_fb_size;
    U8  *ref_framebuffer;
} T_POLYREC_STATE;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  IN-MEMORY LOADED RECORDING (for replay)                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    T_POLYREC_FILE_HEADER header;

    /* CLUT data */
    U8  palette[POLYREC_PALETTE_SIZE];
    U8  rgb_palette[POLYREC_RGB_PALETTE_SIZE];
    U8  has_rgb_palette;
    U32 initial_clut_line;
    U8  *clut_fog;           /* allocated: POLYREC_CLUT_SIZE */
    U8  *clut_gouraud;       /* allocated: POLYREC_CLUT_SIZE */

    /* Texture table */
    U32 num_textures;
    T_POLYREC_MEM_TEXTURE textures[POLYREC_MAX_TEXTURES];

    /* Initial state */
    T_POLYREC_INIT_STATE init_state;

    /* Event stream (raw bytes, played back sequentially) */
    U8  *event_data;
    U32  event_data_size;
    U32  num_events;

    /* Reference framebuffer */
    U32 ref_fb_size;
    U8  *ref_framebuffer;
} T_POLYREC_LOADED;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  RECORDING API (game-side, gated by ENABLE_POLY_RECORDING)                 */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Start recording polygon calls for the current frame.
 * @param filename  Output file path (.lba2polyrec)
 * @return 0 on success, -1 on failure
 */
int polyrec_start(const char *filename);

/**
 * Stop recording and write the .lba2polyrec file to disk.
 * Also captures the game's actual framebuffer for validation.
 * @return 0 on success, -1 on failure
 */
int polyrec_stop(void);

/**
 * @return 1 if currently recording, 0 otherwise.
 */
int polyrec_is_recording(void);

/**
 * Set the RGB palette for PPM output during recording.
 * Must be called between polyrec_start() and polyrec_stop().
 * @param rgb_palette  768 bytes (256 entries × 3 bytes R,G,B)
 */
void polyrec_set_rgb_palette(const U8 *rgb_palette);

/**
 * Record a Fill_Poly call.
 * Call this at the entry of Fill_Poly() when recording is active.
 */
void polyrec_record_fill_poly(S32 type_poly, S32 color_poly, S32 nb_points,
                              const void *ptr_points);

/**
 * Record a Fill_Sphere call.
 */
void polyrec_record_fill_sphere(S32 type_sphere, S32 color_sphere,
                                S32 centre_x, S32 centre_y,
                                S32 rayon, S32 zbuf_value);

/**
 * Record a Line_A call.
 */
void polyrec_record_line_a(S32 x0, S32 y0, S32 x1, S32 y1,
                           S32 col, S32 z1, S32 z2);

/**
 * Record a Switch_Fillers call (only if bank changed).
 */
void polyrec_record_switch_fillers(U32 bank);

/**
 * Record a SetFog call (only if params changed).
 */
void polyrec_record_set_fog(S32 z_near, S32 z_far);

/**
 * Record a SetCLUT call (only if line changed).
 */
void polyrec_record_set_clut(U32 line);

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  LOAD API (test-side, for replay)                                          */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Load a .lba2polyrec file into memory for replay.
 * @param filename  Input file path
 * @param out       Output structure (caller must free with polyrec_free)
 * @return 0 on success, -1 on failure
 */
int polyrec_load(const char *filename, T_POLYREC_LOADED *out);

/**
 * Free all dynamically allocated memory in a loaded recording.
 */
void polyrec_free(T_POLYREC_LOADED *rec);

#ifdef __cplusplus
}
#endif
