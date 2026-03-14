/*
 * POLY_RECORDING.CPP — Polygon Call Recording & Loading
 *
 * Records Fill_Poly/Fill_Sphere/Line_A calls and state changes during
 * gameplay into a .lba2polyrec file for ASM-vs-CPP equivalence testing.
 */

#include <SNAPSHOT/POLY_RECORDING.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  RECORDING STATE                                                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

static int g_recording = 0;
static char g_filename[256];
static T_POLYREC_STATE g_rec;

#define INITIAL_BUF_SIZE  (1024 * 1024)  /* 1 MB */

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  HELPERS                                                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

static int ensure_space(U32 needed) {
    if (g_rec.event_buf_used + needed <= g_rec.event_buf_size)
        return 0;
    U32 new_size = g_rec.event_buf_size * 2;
    while (new_size < g_rec.event_buf_used + needed)
        new_size *= 2;
    U8 *new_buf = (U8 *)realloc(g_rec.event_buf, new_size);
    if (!new_buf) return -1;
    g_rec.event_buf = new_buf;
    g_rec.event_buf_size = new_size;
    return 0;
}

static void emit_u8(U8 v) {
    g_rec.event_buf[g_rec.event_buf_used++] = v;
}

static void emit_u32(U32 v) {
    memcpy(g_rec.event_buf + g_rec.event_buf_used, &v, 4);
    g_rec.event_buf_used += 4;
}

static void emit_s32(S32 v) {
    memcpy(g_rec.event_buf + g_rec.event_buf_used, &v, 4);
    g_rec.event_buf_used += 4;
}

static void emit_bytes(const void *data, U32 size) {
    memcpy(g_rec.event_buf + g_rec.event_buf_used, data, size);
    g_rec.event_buf_used += size;
}

static int write_bytes(FILE *f, const void *data, size_t size) {
    return (fwrite(data, 1, size, f) == size) ? 0 : -1;
}

static int read_bytes(FILE *f, void *data, size_t size) {
    return (fread(data, 1, size, f) == size) ? 0 : -1;
}

static int write_section_header(FILE *f, U32 type, U32 size) {
    T_POLYREC_SECTION_HEADER hdr;
    hdr.type = type;
    hdr.size = size;
    return write_bytes(f, &hdr, sizeof(hdr));
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  TEXTURE TRACKING                                                          */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* Compute texture data size from RepMask.
 * RepMask encodes (width-1) in low byte, (height-1) in high byte.
 * e.g. 0xFFFF → 256×256 = 65536, 0x7F7F → 128×128 = 16384 */
static U32 texture_size_from_repmask(S32 rep_mask) {
    U32 w = (rep_mask & 0xFF) + 1;
    U32 h = ((rep_mask >> 8) & 0xFF) + 1;
    return w * h;
}

/* Find or add a texture to the recording's texture table.
 * Returns the texture index, or -1 on error. */
static S32 find_or_add_texture(U8 *ptr_map, S32 rep_mask) {
    U32 i;
    if (!ptr_map) return -1;

    /* Check if already recorded (by original pointer) */
    for (i = 0; i < g_rec.num_textures; i++) {
        if (g_rec.textures[i].original_ptr == ptr_map)
            return (S32)i;
    }

    /* Add new texture */
    if (g_rec.num_textures >= POLYREC_MAX_TEXTURES)
        return -1;

    U32 idx = g_rec.num_textures;
    U32 data_size = texture_size_from_repmask(rep_mask);

    g_rec.textures[idx].texture_id = idx;
    g_rec.textures[idx].rep_mask = (U32)rep_mask;
    g_rec.textures[idx].data_size = data_size;
    g_rec.textures[idx].data = (U8 *)malloc(data_size);
    if (!g_rec.textures[idx].data) return -1;
    memcpy(g_rec.textures[idx].data, ptr_map, data_size);
    g_rec.textures[idx].original_ptr = ptr_map;

    g_rec.num_textures++;
    return (S32)idx;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  RECORDING API                                                             */
/* ═══════════════════════════════════════════════════════════════════════════ */

int polyrec_start(const char *filename) {
    if (g_recording) return -1;

    memset(&g_rec, 0, sizeof(g_rec));

    /* Allocate event buffer */
    g_rec.event_buf = (U8 *)malloc(INITIAL_BUF_SIZE);
    if (!g_rec.event_buf) return -1;
    g_rec.event_buf_size = INITIAL_BUF_SIZE;
    g_rec.event_buf_used = 0;
    g_rec.num_events = 0;

    /* Capture screen dimensions */
    g_rec.screen_width = (U16)ModeDesiredX;
    g_rec.screen_height = (U16)ModeDesiredY;
    g_rec.screen_pitch = ScreenPitch;

    /* Capture CLUT data */
    memcpy(g_rec.palette, Fill_Logical_Palette, POLYREC_PALETTE_SIZE);
    if (PtrCLUTFog) {
        g_rec.clut_fog = (U8 *)malloc(POLYREC_CLUT_SIZE);
        if (g_rec.clut_fog) memcpy(g_rec.clut_fog, PtrCLUTFog, POLYREC_CLUT_SIZE);
    }
    if (PtrCLUTGouraud) {
        g_rec.clut_gouraud = (U8 *)malloc(POLYREC_CLUT_SIZE);
        if (g_rec.clut_gouraud) memcpy(g_rec.clut_gouraud, PtrCLUTGouraud, POLYREC_CLUT_SIZE);
    }

    /* Compute initial CLUT line from PtrTruePal offset into PtrCLUTFog */
    if (PtrTruePal && PtrCLUTFog && PtrTruePal >= PtrCLUTFog) {
        g_rec.initial_clut_line = (U32)(PtrTruePal - PtrCLUTFog) >> 8;
    } else {
        g_rec.initial_clut_line = 0;
    }

    /* Capture initial state */
    g_rec.init_state.filler_bank = FILL_POLY_TEXTURES; /* default; will be overridden by first Switch_Fillers */
    g_rec.init_state.fog_near = Fill_Z_Fog_Near;
    g_rec.init_state.fog_far = Fill_Z_Fog_Far;
    g_rec.init_state.fill_patch = Fill_Patch;
    g_rec.init_state.active_texture = 0xFFFFFFFF; /* no texture yet */
    g_rec.init_state.rep_mask = RepMask;
    g_rec.init_state.clip_xmin = ClipXMin;
    g_rec.init_state.clip_ymin = ClipYMin;
    g_rec.init_state.clip_xmax = ClipXMax;
    g_rec.init_state.clip_ymax = ClipYMax;

    /* Initialize delta tracking */
    g_rec.last_filler_bank = 0xFFFFFFFF; /* force first Switch_Fillers to be recorded */
    g_rec.last_fog_near = Fill_Z_Fog_Near;
    g_rec.last_fog_far = Fill_Z_Fog_Far;
    g_rec.last_clut_line = g_rec.initial_clut_line;
    g_rec.last_ptr_map = NULL;
    g_rec.last_rep_mask = RepMask;
    g_rec.last_fill_patch = Fill_Patch;
    g_rec.last_clip_xmin = ClipXMin;
    g_rec.last_clip_ymin = ClipYMin;
    g_rec.last_clip_xmax = ClipXMax;
    g_rec.last_clip_ymax = ClipYMax;

    /* Save filename */
    strncpy(g_filename, filename, sizeof(g_filename) - 1);
    g_filename[sizeof(g_filename) - 1] = '\0';

    g_recording = 1;
    return 0;
}

int polyrec_is_recording(void) {
    return g_recording;
}

void polyrec_set_rgb_palette(const U8 *rgb_palette) {
    if (!g_recording || !rgb_palette) return;
    memcpy(g_rec.rgb_palette, rgb_palette, POLYREC_RGB_PALETTE_SIZE);
    g_rec.has_rgb_palette = 1;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  EVENT RECORDING FUNCTIONS                                                 */
/* ═══════════════════════════════════════════════════════════════════════════ */

void polyrec_record_switch_fillers(U32 bank) {
    if (!g_recording) return;
    if (bank == g_rec.last_filler_bank) return;

    if (ensure_space(5) != 0) return;
    emit_u8(POLYREC_EVT_SWITCH_FILLERS);
    emit_u32(bank);
    g_rec.num_events++;
    g_rec.last_filler_bank = bank;

    /* Update initial state if this is the first Switch_Fillers call */
    if (g_rec.init_state.filler_bank == FILL_POLY_TEXTURES && g_rec.num_events == 1) {
        g_rec.init_state.filler_bank = bank;
    }
}

void polyrec_record_set_fog(S32 z_near, S32 z_far) {
    if (!g_recording) return;
    if (z_near == g_rec.last_fog_near && z_far == g_rec.last_fog_far) return;

    if (ensure_space(9) != 0) return;
    emit_u8(POLYREC_EVT_SET_FOG);
    emit_s32(z_near);
    emit_s32(z_far);
    g_rec.num_events++;
    g_rec.last_fog_near = z_near;
    g_rec.last_fog_far = z_far;
}

void polyrec_record_set_clut(U32 line) {
    if (!g_recording) return;
    if (line == g_rec.last_clut_line) return;

    if (ensure_space(5) != 0) return;
    emit_u8(POLYREC_EVT_SET_CLUT);
    emit_u32(line);
    g_rec.num_events++;
    g_rec.last_clut_line = line;
}

/* Check for texture/RepMask/clip/Fill_Patch changes before a draw call */
static void check_state_changes(void) {
    /* Texture change */
    if (PtrMap != g_rec.last_ptr_map) {
        S32 idx = find_or_add_texture(PtrMap, RepMask);
        if (idx >= 0) {
            if (ensure_space(5) == 0) {
                emit_u8(POLYREC_EVT_SET_TEXTURE);
                emit_u32((U32)idx);
                g_rec.num_events++;
            }
        }
        g_rec.last_ptr_map = PtrMap;
    }

    /* RepMask change */
    if (RepMask != g_rec.last_rep_mask) {
        if (ensure_space(5) == 0) {
            emit_u8(POLYREC_EVT_SET_REPMASK);
            emit_u32((U32)RepMask);
            g_rec.num_events++;
        }
        g_rec.last_rep_mask = RepMask;
    }

    /* Clip region change */
    if (ClipXMin != g_rec.last_clip_xmin || ClipYMin != g_rec.last_clip_ymin ||
        ClipXMax != g_rec.last_clip_xmax || ClipYMax != g_rec.last_clip_ymax) {
        if (ensure_space(17) == 0) {
            emit_u8(POLYREC_EVT_SET_CLIP);
            emit_s32(ClipXMin);
            emit_s32(ClipYMin);
            emit_s32(ClipXMax);
            emit_s32(ClipYMax);
            g_rec.num_events++;
        }
        g_rec.last_clip_xmin = ClipXMin;
        g_rec.last_clip_ymin = ClipYMin;
        g_rec.last_clip_xmax = ClipXMax;
        g_rec.last_clip_ymax = ClipYMax;
    }

    /* Fill_Patch change */
    if (Fill_Patch != g_rec.last_fill_patch) {
        if (ensure_space(5) == 0) {
            emit_u8(POLYREC_EVT_SET_FILL_PATCH);
            emit_u32(Fill_Patch);
            g_rec.num_events++;
        }
        g_rec.last_fill_patch = Fill_Patch;
    }
}

void polyrec_record_fill_poly(S32 type_poly, S32 color_poly, S32 nb_points,
                              const void *ptr_points) {
    U32 points_size;
    if (!g_recording) return;

    check_state_changes();

    /* 1 (type byte) + 4 (type_poly) + 4 (color_poly) + 4 (nb_points) + 16*nb_points */
    points_size = (U32)nb_points * 16;
    if (ensure_space(13 + points_size) != 0) return;

    emit_u8(POLYREC_EVT_FILL_POLY);
    emit_s32(type_poly);
    emit_s32(color_poly);
    emit_s32(nb_points);
    emit_bytes(ptr_points, points_size);
    g_rec.num_events++;
}

void polyrec_record_fill_sphere(S32 type_sphere, S32 color_sphere,
                                S32 centre_x, S32 centre_y,
                                S32 rayon, S32 zbuf_value) {
    if (!g_recording) return;

    check_state_changes();

    if (ensure_space(25) != 0) return;
    emit_u8(POLYREC_EVT_FILL_SPHERE);
    emit_s32(type_sphere);
    emit_s32(color_sphere);
    emit_s32(centre_x);
    emit_s32(centre_y);
    emit_s32(rayon);
    emit_s32(zbuf_value);
    g_rec.num_events++;
}

void polyrec_record_line_a(S32 x0, S32 y0, S32 x1, S32 y1,
                           S32 col, S32 z1, S32 z2) {
    if (!g_recording) return;

    check_state_changes();

    if (ensure_space(29) != 0) return;
    emit_u8(POLYREC_EVT_LINE_A);
    emit_s32(x0);
    emit_s32(y0);
    emit_s32(x1);
    emit_s32(y1);
    emit_s32(col);
    emit_s32(z1);
    emit_s32(z2);
    g_rec.num_events++;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  STOP & WRITE FILE                                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

int polyrec_stop(void) {
    FILE *f;
    U32 i;
    U32 section_size;

    if (!g_recording) return -1;
    g_recording = 0;

    /* Capture reference framebuffer (game's actual output) */
    if (Log) {
        U32 fb_size = (U32)g_rec.screen_width * (U32)g_rec.screen_height;
        g_rec.ref_fb_size = fb_size;
        g_rec.ref_framebuffer = (U8 *)malloc(fb_size);
        if (g_rec.ref_framebuffer) {
            memcpy(g_rec.ref_framebuffer, Log, fb_size);
        }
    }

    /* Emit EOF event */
    if (ensure_space(1) == 0) {
        emit_u8(POLYREC_EVT_EOF);
        g_rec.num_events++;
    }

    /* Write file */
    f = fopen(g_filename, "wb");
    if (!f) goto cleanup;

    /* ── File header ── */
    {
        T_POLYREC_FILE_HEADER hdr;
        memcpy(hdr.magic, POLYREC_MAGIC, POLYREC_MAGIC_SIZE);
        hdr.version = POLYREC_VERSION;
        hdr.screen_width = g_rec.screen_width;
        hdr.screen_height = g_rec.screen_height;
        hdr.screen_pitch = g_rec.screen_pitch;
        hdr.num_events = g_rec.num_events;
        hdr.clip_xmin = g_rec.init_state.clip_xmin;
        hdr.clip_ymin = g_rec.init_state.clip_ymin;
        hdr.clip_xmax = g_rec.init_state.clip_xmax;
        hdr.clip_ymax = g_rec.init_state.clip_ymax;
        if (write_bytes(f, &hdr, sizeof(hdr)) != 0) goto fail;
    }

    /* ── CLUT section ── */
    {
        U32 clut_data_size = 0;
        T_POLYREC_CLUT_HEADER clut_hdr;

        memcpy(clut_hdr.palette, g_rec.palette, POLYREC_PALETTE_SIZE);
        memcpy(clut_hdr.rgb_palette, g_rec.rgb_palette, POLYREC_RGB_PALETTE_SIZE);
        clut_hdr.has_rgb_palette = g_rec.has_rgb_palette;
        memset(clut_hdr._pad, 0, sizeof(clut_hdr._pad));
        clut_hdr.initial_clut_line = g_rec.initial_clut_line;

        clut_data_size = sizeof(clut_hdr);
        if (g_rec.clut_fog) clut_data_size += POLYREC_CLUT_SIZE;
        if (g_rec.clut_gouraud) clut_data_size += POLYREC_CLUT_SIZE;

        if (write_section_header(f, POLYREC_SECTION_CLUT, clut_data_size) != 0) goto fail;
        if (write_bytes(f, &clut_hdr, sizeof(clut_hdr)) != 0) goto fail;
        if (g_rec.clut_fog) {
            if (write_bytes(f, g_rec.clut_fog, POLYREC_CLUT_SIZE) != 0) goto fail;
        }
        if (g_rec.clut_gouraud) {
            if (write_bytes(f, g_rec.clut_gouraud, POLYREC_CLUT_SIZE) != 0) goto fail;
        }
    }

    /* ── TEXTURES section ── */
    {
        T_POLYREC_TEXTURES_HEADER tex_hdr;
        tex_hdr.num_textures = g_rec.num_textures;

        section_size = sizeof(tex_hdr);
        for (i = 0; i < g_rec.num_textures; i++) {
            section_size += sizeof(T_POLYREC_TEXTURE_ENTRY) + g_rec.textures[i].data_size;
        }

        if (write_section_header(f, POLYREC_SECTION_TEXTURES, section_size) != 0) goto fail;
        if (write_bytes(f, &tex_hdr, sizeof(tex_hdr)) != 0) goto fail;

        for (i = 0; i < g_rec.num_textures; i++) {
            T_POLYREC_TEXTURE_ENTRY entry;
            entry.texture_id = g_rec.textures[i].texture_id;
            entry.rep_mask = g_rec.textures[i].rep_mask;
            entry.data_size = g_rec.textures[i].data_size;
            if (write_bytes(f, &entry, sizeof(entry)) != 0) goto fail;
            if (write_bytes(f, g_rec.textures[i].data, g_rec.textures[i].data_size) != 0) goto fail;
        }
    }

    /* ── INIT_STATE section ── */
    if (write_section_header(f, POLYREC_SECTION_INIT_STATE, sizeof(T_POLYREC_INIT_STATE)) != 0) goto fail;
    if (write_bytes(f, &g_rec.init_state, sizeof(T_POLYREC_INIT_STATE)) != 0) goto fail;

    /* ── EVENTS section ── */
    {
        U32 events_section_size = sizeof(U32) + g_rec.event_buf_used;
        if (write_section_header(f, POLYREC_SECTION_EVENTS, events_section_size) != 0) goto fail;
        if (write_bytes(f, &g_rec.num_events, sizeof(U32)) != 0) goto fail;
        if (write_bytes(f, g_rec.event_buf, g_rec.event_buf_used) != 0) goto fail;
    }

    /* ── REF_FB section (game's actual framebuffer) ── */
    if (g_rec.ref_framebuffer && g_rec.ref_fb_size > 0) {
        if (write_section_header(f, POLYREC_SECTION_REF_FB, g_rec.ref_fb_size) != 0) goto fail;
        if (write_bytes(f, g_rec.ref_framebuffer, g_rec.ref_fb_size) != 0) goto fail;
    }

    /* ── END section ── */
    if (write_section_header(f, POLYREC_SECTION_END, 0) != 0) goto fail;

    fclose(f);
    goto cleanup;

fail:
    fclose(f);
    /* Fall through to cleanup */

cleanup:
    /* Free all recording memory */
    if (g_rec.event_buf) { free(g_rec.event_buf); g_rec.event_buf = NULL; }
    if (g_rec.clut_fog) { free(g_rec.clut_fog); g_rec.clut_fog = NULL; }
    if (g_rec.clut_gouraud) { free(g_rec.clut_gouraud); g_rec.clut_gouraud = NULL; }
    if (g_rec.ref_framebuffer) { free(g_rec.ref_framebuffer); g_rec.ref_framebuffer = NULL; }
    for (i = 0; i < g_rec.num_textures; i++) {
        if (g_rec.textures[i].data) { free(g_rec.textures[i].data); g_rec.textures[i].data = NULL; }
    }
    memset(&g_rec, 0, sizeof(g_rec));

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  LOAD — reads a .lba2polyrec file into memory for replay                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

int polyrec_load(const char *filename, T_POLYREC_LOADED *out) {
    FILE *f;
    int done = 0;

    memset(out, 0, sizeof(T_POLYREC_LOADED));

    f = fopen(filename, "rb");
    if (!f) return -1;

    /* Read file header */
    if (read_bytes(f, &out->header, sizeof(T_POLYREC_FILE_HEADER)) != 0) goto fail;
    if (memcmp(out->header.magic, POLYREC_MAGIC, POLYREC_MAGIC_SIZE) != 0) goto fail;
    if (out->header.version != POLYREC_VERSION) goto fail;

    /* Read sections */
    while (!done) {
        T_POLYREC_SECTION_HEADER sec_hdr;
        if (read_bytes(f, &sec_hdr, sizeof(sec_hdr)) != 0) goto fail;

        switch (sec_hdr.type) {

        case POLYREC_SECTION_CLUT: {
            T_POLYREC_CLUT_HEADER clut_hdr;
            U32 remaining;
            if (sec_hdr.size < sizeof(clut_hdr)) goto fail;
            if (read_bytes(f, &clut_hdr, sizeof(clut_hdr)) != 0) goto fail;

            memcpy(out->palette, clut_hdr.palette, POLYREC_PALETTE_SIZE);
            memcpy(out->rgb_palette, clut_hdr.rgb_palette, POLYREC_RGB_PALETTE_SIZE);
            out->has_rgb_palette = clut_hdr.has_rgb_palette;
            out->initial_clut_line = clut_hdr.initial_clut_line;

            remaining = sec_hdr.size - sizeof(clut_hdr);
            if (remaining >= POLYREC_CLUT_SIZE) {
                out->clut_fog = (U8 *)malloc(POLYREC_CLUT_SIZE);
                if (!out->clut_fog) goto fail;
                if (read_bytes(f, out->clut_fog, POLYREC_CLUT_SIZE) != 0) goto fail;
                remaining -= POLYREC_CLUT_SIZE;
            }
            if (remaining >= POLYREC_CLUT_SIZE) {
                out->clut_gouraud = (U8 *)malloc(POLYREC_CLUT_SIZE);
                if (!out->clut_gouraud) goto fail;
                if (read_bytes(f, out->clut_gouraud, POLYREC_CLUT_SIZE) != 0) goto fail;
                remaining -= POLYREC_CLUT_SIZE;
            }
            /* Skip any extra bytes */
            if (remaining > 0) {
                if (fseek(f, (long)remaining, SEEK_CUR) != 0) goto fail;
            }
            break;
        }

        case POLYREC_SECTION_TEXTURES: {
            T_POLYREC_TEXTURES_HEADER tex_hdr;
            U32 j;
            if (read_bytes(f, &tex_hdr, sizeof(tex_hdr)) != 0) goto fail;
            if (tex_hdr.num_textures > POLYREC_MAX_TEXTURES) goto fail;
            out->num_textures = tex_hdr.num_textures;

            for (j = 0; j < tex_hdr.num_textures; j++) {
                T_POLYREC_TEXTURE_ENTRY entry;
                if (read_bytes(f, &entry, sizeof(entry)) != 0) goto fail;
                out->textures[j].texture_id = entry.texture_id;
                out->textures[j].rep_mask = entry.rep_mask;
                out->textures[j].data_size = entry.data_size;
                out->textures[j].data = (U8 *)malloc(entry.data_size);
                if (!out->textures[j].data) goto fail;
                if (read_bytes(f, out->textures[j].data, entry.data_size) != 0) goto fail;
                out->textures[j].original_ptr = NULL; /* not meaningful for loaded data */
            }
            break;
        }

        case POLYREC_SECTION_INIT_STATE:
            if (sec_hdr.size != sizeof(T_POLYREC_INIT_STATE)) goto fail;
            if (read_bytes(f, &out->init_state, sizeof(T_POLYREC_INIT_STATE)) != 0) goto fail;
            break;

        case POLYREC_SECTION_EVENTS: {
            U32 num_events;
            U32 data_size;
            if (sec_hdr.size < sizeof(U32)) goto fail;
            if (read_bytes(f, &num_events, sizeof(U32)) != 0) goto fail;
            out->num_events = num_events;
            data_size = sec_hdr.size - sizeof(U32);
            if (data_size > 0) {
                out->event_data = (U8 *)malloc(data_size);
                if (!out->event_data) goto fail;
                if (read_bytes(f, out->event_data, data_size) != 0) goto fail;
                out->event_data_size = data_size;
            }
            break;
        }

        case POLYREC_SECTION_REF_FB:
            if (sec_hdr.size > 0) {
                out->ref_fb_size = sec_hdr.size;
                out->ref_framebuffer = (U8 *)malloc(sec_hdr.size);
                if (!out->ref_framebuffer) goto fail;
                if (read_bytes(f, out->ref_framebuffer, sec_hdr.size) != 0) goto fail;
            }
            break;

        case POLYREC_SECTION_END:
            done = 1;
            break;

        default:
            /* Unknown section: skip */
            if (fseek(f, (long)sec_hdr.size, SEEK_CUR) != 0) goto fail;
            break;
        }
    }

    fclose(f);
    return 0;

fail:
    fclose(f);
    polyrec_free(out);
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  FREE                                                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

void polyrec_free(T_POLYREC_LOADED *rec) {
    U32 i;

    if (rec->clut_fog) { free(rec->clut_fog); rec->clut_fog = NULL; }
    if (rec->clut_gouraud) { free(rec->clut_gouraud); rec->clut_gouraud = NULL; }
    if (rec->event_data) { free(rec->event_data); rec->event_data = NULL; }
    if (rec->ref_framebuffer) { free(rec->ref_framebuffer); rec->ref_framebuffer = NULL; }

    for (i = 0; i < rec->num_textures; i++) {
        if (rec->textures[i].data) { free(rec->textures[i].data); rec->textures[i].data = NULL; }
    }

    memset(rec, 0, sizeof(T_POLYREC_LOADED));
}
