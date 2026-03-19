/*
 * polyrec_replay.cpp — Polygon recording replay logic
 *
 * Loads a .lba2polyrec file, applies initial rendering state,
 * replays all polygon calls (Fill_Poly, Fill_Sphere, Line_A),
 * and writes the raw framebuffer to a file.
 *
 * Used by both replay_polyrec_asm and replay_polyrec_cpp.
 * The only difference is which library (ASM or CPP) is linked.
 */

#include "polyrec_replay.h"

#include <SNAPSHOT/POLY_RECORDING.H>
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  FRAMEBUFFER ALLOCATION                                                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

static U8 *framebuffer = NULL;
static U16 *zbuffer = NULL;
static U32 fb_size = 0;
static U32 zb_size = 0;

static int allocate_buffers(U32 width, U32 height) {
    fb_size = width * height; /* 8-bit indexed color */
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
    if (framebuffer) {
        free(framebuffer);
        framebuffer = NULL;
    }
    if (zbuffer) {
        free(zbuffer);
        zbuffer = NULL;
    }
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
        fputc(rgb_palette[idx * 3 + 0], f); /* R */
        fputc(rgb_palette[idx * 3 + 1], f); /* G */
        fputc(rgb_palette[idx * 3 + 2], f); /* B */
    }
    fclose(f);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  EVENT STREAM READER HELPERS                                               */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const U8 *data;
    U32 size;
    U32 pos;
} T_EVENT_READER;

static U8 read_u8(T_EVENT_READER *r) {
    if (r->pos >= r->size)
        return POLYREC_EVT_EOF;
    return r->data[r->pos++];
}

static U32 read_u32(T_EVENT_READER *r) {
    U32 v;
    if (r->pos + 4 > r->size)
        return 0;
    memcpy(&v, r->data + r->pos, 4);
    r->pos += 4;
    return v;
}

static S32 read_s32(T_EVENT_READER *r) {
    S32 v;
    if (r->pos + 4 > r->size)
        return 0;
    memcpy(&v, r->data + r->pos, 4);
    r->pos += 4;
    return v;
}

static const U8 *read_bytes(T_EVENT_READER *r, U32 size) {
    const U8 *p;
    if (r->pos + size > r->size)
        return NULL;
    p = r->data + r->pos;
    r->pos += size;
    return p;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  MAIN REPLAY ENTRY POINT                                                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

int polyrec_replay_run(const char *polyrec_file, const char *output_file,
                       const char *ppm_file, const char *ref_ppm_file,
                       int start_after, int stop_after) {
    T_POLYREC_LOADED rec;
    T_EVENT_READER reader;
    FILE *f;
    U32 draw_count = 0;
    int done = 0;

    /* Debug slope output: set POLYREC_DEBUG_SLOPES=from,to to enable */
    int debug_slopes = 0;
    int debug_slopes_from = 0;
    int debug_slopes_to = 0;
    {
        const char *env = getenv("POLYREC_DEBUG_SLOPES");
        if (env) {
            debug_slopes = 1;
            sscanf(env, "%d,%d", &debug_slopes_from, &debug_slopes_to);
            if (debug_slopes_to == 0)
                debug_slopes_to = debug_slopes_from;
        }
    }

    /* Load recording */
    if (polyrec_load(polyrec_file, &rec) != 0) {
        fprintf(stderr, "ERROR: Failed to load polygon recording '%s'\n", polyrec_file);
        return 1;
    }

    fprintf(stderr, "INFO: Loaded polyrec: %ux%u, %u events, %u textures\n",
            rec.header.screen_width, rec.header.screen_height,
            rec.num_events, rec.num_textures);

    /* Allocate framebuffer and Z-buffer */
    if (allocate_buffers(rec.header.screen_width, rec.header.screen_height) != 0) {
        polyrec_free(&rec);
        return 1;
    }

    /* Build TabOffLine for our framebuffer */
    {
        U32 y;
        for (y = 0; y < rec.header.screen_height && y < 1024; y++) {
            TabOffLine[y] = y * rec.header.screen_width;
        }
        PTR_TabOffLine = TabOffLine;
        ScreenPitch = rec.header.screen_width;
    }

    /* Set screen dimensions */
    ModeDesiredX = rec.header.screen_width;
    ModeDesiredY = rec.header.screen_height;

    /* Apply CLUT data */
    if (rec.clut_fog) {
        PtrCLUTFog = rec.clut_fog;
    }
    if (rec.clut_gouraud) {
        PtrCLUTGouraud = rec.clut_gouraud;
    }
    memcpy(Fill_Logical_Palette, rec.palette, POLYREC_PALETTE_SIZE);

    /* Apply initial CLUT line (sets PtrTruePal) */
    if (PtrCLUTFog) {
        PtrTruePal = PtrCLUTFog + (rec.initial_clut_line << 8);
    }

    /* Set clipping */
    ClipXMin = rec.init_state.clip_xmin;
    ClipYMin = rec.init_state.clip_ymin;
    ClipXMax = rec.init_state.clip_xmax;
    ClipYMax = rec.init_state.clip_ymax;

    /* Apply initial state */
    Fill_Patch = rec.init_state.fill_patch;
    RepMask = rec.init_state.rep_mask;

    /* Initialize filler dispatch table */
    Switch_Fillers(rec.init_state.filler_bank);

    /* Set fog */
    SetFog(rec.init_state.fog_near, rec.init_state.fog_far);

    /* Apply initial texture if set */
    if (rec.init_state.active_texture != 0xFFFFFFFF &&
        rec.init_state.active_texture < rec.num_textures) {
        PtrMap = rec.textures[rec.init_state.active_texture].data;
        RepMask = rec.textures[rec.init_state.active_texture].rep_mask;
    }

    fprintf(stderr, "INFO: Replaying events (bank=%u, fog_near=%d, fog_far=%d, "
                    "clip=(%d,%d)-(%d,%d), start_after=%d, stop_after=%d)\n",
            rec.init_state.filler_bank,
            rec.init_state.fog_near, rec.init_state.fog_far,
            rec.init_state.clip_xmin, rec.init_state.clip_ymin,
            rec.init_state.clip_xmax, rec.init_state.clip_ymax,
            start_after,
            stop_after);

    /* Process event stream */
    reader.data = rec.event_data;
    reader.size = rec.event_data_size;
    reader.pos = 0;

    while (!done && reader.pos < reader.size) {
        U8 evt_type = read_u8(&reader);

        switch (evt_type) {
        case POLYREC_EVT_SWITCH_FILLERS: {
            U32 bank = read_u32(&reader);
            Switch_Fillers(bank);
            break;
        }

        case POLYREC_EVT_SET_FOG: {
            S32 z_near = read_s32(&reader);
            S32 z_far = read_s32(&reader);
            SetFog(z_near, z_far);
            break;
        }

        case POLYREC_EVT_SET_CLUT: {
            U32 line = read_u32(&reader);
            SetCLUT(line);
            break;
        }

        case POLYREC_EVT_SET_TEXTURE: {
            U32 tex_idx = read_u32(&reader);
            if (tex_idx < rec.num_textures) {
                PtrMap = rec.textures[tex_idx].data;
                RepMask = rec.textures[tex_idx].rep_mask;
            }
            break;
        }

        case POLYREC_EVT_SET_CLIP: {
            ClipXMin = read_s32(&reader);
            ClipYMin = read_s32(&reader);
            ClipXMax = read_s32(&reader);
            ClipYMax = read_s32(&reader);
            break;
        }

        case POLYREC_EVT_SET_REPMASK: {
            RepMask = read_u32(&reader);
            break;
        }

        case POLYREC_EVT_SET_FILL_PATCH: {
            Fill_Patch = read_u32(&reader);
            break;
        }

        case POLYREC_EVT_SET_CLUT_OFFSET: {
            U32 clut_offset = read_u32(&reader);
            if (rec.clut_fog && clut_offset < POLYREC_CLUT_SIZE) {
                PtrCLUTGouraud = rec.clut_fog + clut_offset;
            }
            break;
        }

        case POLYREC_EVT_FILL_POLY: {
            S32 type_poly = read_s32(&reader);
            S32 color_poly = read_s32(&reader);
            S32 nb_points = read_s32(&reader);
            U32 points_size = (U32)nb_points * 16;
            const U8 *points_data = read_bytes(&reader, points_size);

            if (points_data && nb_points > 0 && (start_after < 0 || (int)draw_count >= start_after)) {
                /* Copy to local buffer (Fill_Poly may modify points in-place during clipping) */
                Struc_Point local_points[64]; /* max 64 vertices should be enough */
                U32 copy_count = (U32)nb_points;
                if (copy_count > 64)
                    copy_count = 64;
                memcpy(local_points, points_data, copy_count * 16);

                /* Debug: dump input data before Fill_Poly */
                if (debug_slopes && (int)draw_count + 1 >= debug_slopes_from && (int)draw_count + 1 <= debug_slopes_to) {
                    fprintf(stderr, "DC%u type=%d color=%d nb_points=%d RepMask=%d\n",
                            draw_count + 1, type_poly, color_poly, nb_points, RepMask);
                    for (int pi = 0; pi < nb_points && pi < 8; pi++) {
                        fprintf(stderr, "  pt[%d]: XE=%d YE=%d MapU=%u MapV=%u Light=%u ZO=%u W=%d\n",
                                pi, local_points[pi].Pt_XE, local_points[pi].Pt_YE,
                                (U32)local_points[pi].Pt_MapU, (U32)local_points[pi].Pt_MapV,
                                (U32)local_points[pi].Pt_Light, (U32)local_points[pi].Pt_ZO,
                                local_points[pi].Pt_W);
                    }
                }

                Fill_Poly(type_poly, color_poly, nb_points, local_points);

                /* Debug: dump slope globals after Fill_Poly for draw call comparison */
                if (debug_slopes && (int)draw_count + 1 >= debug_slopes_from && (int)draw_count + 1 <= debug_slopes_to) {
                    fprintf(stderr, "DC%u slopes: LS=%d RS=%d UXS=%d VXS=%d ULS=%d VLS=%d GXS=%d GLS=%d ZXS=%d ZLS=%d\n",
                            draw_count + 1,
                            Fill_LeftSlope, Fill_RightSlope,
                            Fill_MapU_XSlope, Fill_MapV_XSlope,
                            Fill_MapU_LeftSlope, Fill_MapV_LeftSlope,
                            Fill_Gouraud_XSlope, Fill_Gouraud_LeftSlope,
                            Fill_ZBuf_XSlope, Fill_ZBuf_LeftSlope);
                    fprintf(stderr, "DC%u init: UMin=%d VMin=%d GMin=%d ZMin=%u\n",
                            draw_count + 1,
                            Fill_CurMapUMin, Fill_CurMapVMin,
                            Fill_CurGouraudMin, Fill_CurZBufMin);
                }
            }

            draw_count++;
            if (stop_after >= 0 && (int)draw_count >= stop_after) {
                done = 1;
            }
            break;
        }

        case POLYREC_EVT_FILL_SPHERE: {
            S32 type_sphere = read_s32(&reader);
            S32 color_sphere = read_s32(&reader);
            S32 centre_x = read_s32(&reader);
            S32 centre_y = read_s32(&reader);
            S32 rayon = read_s32(&reader);
            S32 zbuf_value = read_s32(&reader);

            if (start_after < 0 || (int)draw_count >= start_after) {
                Fill_Sphere(type_sphere, color_sphere, centre_x, centre_y,
                            rayon, zbuf_value);
            }

            draw_count++;
            if (stop_after >= 0 && (int)draw_count >= stop_after) {
                done = 1;
            }
            break;
        }

        case POLYREC_EVT_LINE_A: {
            S32 x0 = read_s32(&reader);
            S32 y0 = read_s32(&reader);
            S32 x1 = read_s32(&reader);
            S32 y1 = read_s32(&reader);
            S32 col = read_s32(&reader);
            S32 z1 = read_s32(&reader);
            S32 z2 = read_s32(&reader);

            if (start_after < 0 || (int)draw_count >= start_after) {
                Line_A(x0, y0, x1, y1, col, z1, z2);
            }

            draw_count++;
            if (stop_after >= 0 && (int)draw_count >= stop_after) {
                done = 1;
            }
            break;
        }

        case POLYREC_EVT_EOF:
            done = 1;
            break;

        default:
            fprintf(stderr, "WARNING: Unknown event type 0x%02X at offset %u\n",
                    evt_type, reader.pos - 1);
            done = 1;
            break;
        }
    }

    fprintf(stderr, "INFO: Replayed %u draw calls\n", draw_count);

    /* Count non-zero pixels in framebuffer */
    {
        U32 nonzero = 0, total = fb_size, i;
        for (i = 0; i < total; i++) {
            if (framebuffer[i] != 0)
                nonzero++;
        }
        fprintf(stderr, "INFO: Framebuffer: %u/%u non-zero pixels (%.1f%%)\n",
                nonzero, total, 100.0 * nonzero / total);
    }

    /* Write raw framebuffer to output file */
    f = fopen(output_file, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Failed to open output file '%s'\n", output_file);
        free_buffers();
        polyrec_free(&rec);
        return 1;
    }
    fwrite(framebuffer, 1, fb_size, f);
    fclose(f);

    /* Write Z-buffer to a separate file (replace .raw with _zbuf.raw) */
    if (zbuffer) {
        char zbuf_file[512];
        snprintf(zbuf_file, sizeof(zbuf_file), "%s", output_file);
        char *ext = strstr(zbuf_file, ".raw");
        if (ext) {
            snprintf(ext, zbuf_file + sizeof(zbuf_file) - ext, "_zbuf.raw");
        } else {
            snprintf(zbuf_file + strlen(zbuf_file),
                     sizeof(zbuf_file) - strlen(zbuf_file), "_zbuf.raw");
        }
        FILE *zf = fopen(zbuf_file, "wb");
        if (zf) {
            fwrite(zbuffer, 2, fb_size, zf);
            fclose(zf);
        }
    }

    /* Write PPM image if requested and palette is available */
    if (ppm_file) {
        if (rec.has_rgb_palette) {
            write_ppm(ppm_file, framebuffer,
                      rec.header.screen_width, rec.header.screen_height,
                      rec.rgb_palette);
            fprintf(stderr, "INFO: Wrote PPM image: %s\n", ppm_file);
        } else {
            fprintf(stderr, "WARNING: Recording has no RGB palette, skipping PPM output\n");
        }
    }

    /* Write game reference framebuffer if requested and available */
    if (ref_ppm_file) {
        if (rec.ref_framebuffer && rec.ref_fb_size > 0 && rec.has_rgb_palette) {
            write_ppm(ref_ppm_file, rec.ref_framebuffer,
                      rec.header.screen_width, rec.header.screen_height,
                      rec.rgb_palette);
            fprintf(stderr, "INFO: Wrote game reference image: %s\n", ref_ppm_file);
        } else {
            fprintf(stderr, "WARNING: No reference framebuffer in recording, skipping\n");
        }
    }

    /* Cleanup */
    free_buffers();
    polyrec_free(&rec);

    return 0;
}
