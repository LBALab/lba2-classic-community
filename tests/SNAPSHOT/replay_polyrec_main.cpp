/*
 * replay_polyrec_main.cpp — Main entry point for polygon recording replay
 *
 * This file is compiled into two executables:
 *   - replay_polyrec_asm  (linked against ASM library)
 *   - replay_polyrec_cpp  (linked against CPP library)
 *
 * Usage: replay_polyrec_{asm,cpp} <recording.lba2polyrec> <output.raw>
 *            [--ppm output.ppm] [--ref-ppm ref.ppm] [--stop-after N]
 */

#include "polyrec_replay.h"
#include <SNAPSHOT/POLY_RECORDING.H>
#include <POLYGON/POLY.H>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Dump mode: print event stream without rendering ───────────────────── */
static const char *evt_name(U8 type) {
    switch (type) {
        case POLYREC_EVT_SWITCH_FILLERS: return "SWITCH_FILLERS";
        case POLYREC_EVT_SET_FOG:        return "SET_FOG";
        case POLYREC_EVT_SET_CLUT:       return "SET_CLUT";
        case POLYREC_EVT_SET_TEXTURE:    return "SET_TEXTURE";
        case POLYREC_EVT_SET_CLIP:       return "SET_CLIP";
        case POLYREC_EVT_SET_REPMASK:    return "SET_REPMASK";
        case POLYREC_EVT_SET_FILL_PATCH: return "SET_FILL_PATCH";
        case POLYREC_EVT_FILL_POLY:      return "FILL_POLY";
        case POLYREC_EVT_FILL_SPHERE:    return "FILL_SPHERE";
        case POLYREC_EVT_LINE_A:         return "LINE_A";
        case POLYREC_EVT_EOF:            return "EOF";
        default:                         return "UNKNOWN";
    }
}

static int dump_recording(const char *filename) {
    T_POLYREC_LOADED rec;
    U32 pos = 0, evt_idx = 0, draw_count = 0;
    U32 poly_type_counts[32] = {0};

    if (polyrec_load(filename, &rec) != 0) {
        fprintf(stderr, "ERROR: Failed to load '%s'\n", filename);
        return 1;
    }

    printf("=== POLYREC DUMP: %s ===\n", filename);
    printf("Screen: %ux%u, pitch=%u\n", rec.header.screen_width,
           rec.header.screen_height, rec.header.screen_pitch);
    printf("Clip: (%d,%d)-(%d,%d)\n", rec.header.clip_xmin, rec.header.clip_ymin,
           rec.header.clip_xmax, rec.header.clip_ymax);
    printf("Events: %u\n", rec.num_events);
    printf("Textures: %u\n", rec.num_textures);
    for (U32 t = 0; t < rec.num_textures; t++) {
        printf("  Texture[%u]: rep_mask=0x%04X, size=%u bytes\n",
               t, rec.textures[t].rep_mask, rec.textures[t].data_size);
    }
    printf("Init state: bank=%u, fog_near=%d, fog_far=%d, fill_patch=%u\n",
           rec.init_state.filler_bank, rec.init_state.fog_near,
           rec.init_state.fog_far, rec.init_state.fill_patch);
    printf("Init state: texture=%u, rep_mask=0x%04X\n",
           rec.init_state.active_texture, rec.init_state.rep_mask);
    printf("Init state: clip=(%d,%d)-(%d,%d)\n",
           rec.init_state.clip_xmin, rec.init_state.clip_ymin,
           rec.init_state.clip_xmax, rec.init_state.clip_ymax);
    printf("Has RGB palette: %d\n", rec.has_rgb_palette);
    printf("Has ref framebuffer: %d (%u bytes)\n",
           rec.ref_framebuffer ? 1 : 0, rec.ref_fb_size);
    printf("\n--- Event Stream ---\n");

    while (pos < rec.event_data_size) {
        U8 type = rec.event_data[pos++];
        printf("[%4u] %s", evt_idx, evt_name(type));

        switch (type) {
        case POLYREC_EVT_SWITCH_FILLERS: {
            U32 bank; memcpy(&bank, rec.event_data + pos, 4); pos += 4;
            printf("(%u)\n", bank);
            break;
        }
        case POLYREC_EVT_SET_FOG: {
            S32 zn, zf;
            memcpy(&zn, rec.event_data + pos, 4); pos += 4;
            memcpy(&zf, rec.event_data + pos, 4); pos += 4;
            printf("(near=%d, far=%d)\n", zn, zf);
            break;
        }
        case POLYREC_EVT_SET_CLUT: {
            U32 line; memcpy(&line, rec.event_data + pos, 4); pos += 4;
            printf("(%u)\n", line);
            break;
        }
        case POLYREC_EVT_SET_TEXTURE: {
            U32 idx; memcpy(&idx, rec.event_data + pos, 4); pos += 4;
            printf("(%u)\n", idx);
            break;
        }
        case POLYREC_EVT_SET_CLIP: {
            S32 v[4];
            memcpy(v, rec.event_data + pos, 16); pos += 16;
            printf("(%d,%d)-(%d,%d)\n", v[0], v[1], v[2], v[3]);
            break;
        }
        case POLYREC_EVT_SET_REPMASK: {
            U32 rm; memcpy(&rm, rec.event_data + pos, 4); pos += 4;
            printf("(0x%04X)\n", rm);
            break;
        }
        case POLYREC_EVT_SET_FILL_PATCH: {
            U32 fp; memcpy(&fp, rec.event_data + pos, 4); pos += 4;
            printf("(%u)\n", fp);
            break;
        }
        case POLYREC_EVT_FILL_POLY: {
            S32 tp, cp, nb;
            memcpy(&tp, rec.event_data + pos, 4); pos += 4;
            memcpy(&cp, rec.event_data + pos, 4); pos += 4;
            memcpy(&nb, rec.event_data + pos, 4); pos += 4;
            printf("(type=%d, color=%d, pts=%d)", tp, cp, nb);
            /* Print vertex data: Struc_Point is 16 bytes packed */
            for (S32 v = 0; v < nb && v < 8; v++) {
                S16 xe, ye; U16 mu, mv, light, zo; S32 w;
                memcpy(&xe,    rec.event_data + pos + v*16 + 0, 2);
                memcpy(&ye,    rec.event_data + pos + v*16 + 2, 2);
                memcpy(&mu,    rec.event_data + pos + v*16 + 4, 2);
                memcpy(&mv,    rec.event_data + pos + v*16 + 6, 2);
                memcpy(&light, rec.event_data + pos + v*16 + 8, 2);
                memcpy(&zo,    rec.event_data + pos + v*16 + 10, 2);
                memcpy(&w,     rec.event_data + pos + v*16 + 12, 4);
                printf("\n        v[%d]: xy=(%d,%d) uv=(%u,%u) light=%u zo=%u w=%d",
                       v, xe, ye, mu, mv, light, zo, w);
            }
            printf("\n");
            pos += (U32)nb * 16;
            if (tp >= 0 && tp < 32) poly_type_counts[tp]++;
            draw_count++;
            break;
        }
        case POLYREC_EVT_FILL_SPHERE: {
            S32 v[6];
            memcpy(v, rec.event_data + pos, 24); pos += 24;
            printf("(type=%d, color=%d, cx=%d, cy=%d, r=%d, z=%d)\n",
                   v[0], v[1], v[2], v[3], v[4], v[5]);
            draw_count++;
            break;
        }
        case POLYREC_EVT_LINE_A: {
            S32 v[7];
            memcpy(v, rec.event_data + pos, 28); pos += 28;
            printf("(x0=%d,y0=%d,x1=%d,y1=%d,col=%d,z1=%d,z2=%d)\n",
                   v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
            draw_count++;
            break;
        }
        case POLYREC_EVT_EOF:
            printf("\n");
            goto done;
        default:
            printf(" (unknown type 0x%02X)\n", type);
            goto done;
        }
        evt_idx++;
    }
done:
    printf("\n--- Summary ---\n");
    printf("Total events: %u, draw calls: %u\n", evt_idx, draw_count);
    printf("Poly type distribution:\n");
    for (int t = 0; t < 32; t++) {
        if (poly_type_counts[t] > 0)
            printf("  Type %2d: %u calls\n", t, poly_type_counts[t]);
    }

    polyrec_free(&rec);
    return 0;
}

/* ── Main ──────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    const char *ppm_file = NULL;
    const char *ref_ppm_file = NULL;
    int stop_after = -1;  /* -1 = replay all */
    int dump_mode = 0;
    int i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <recording.lba2polyrec> <output.raw> "
                "[--ppm output.ppm] [--ref-ppm ref.ppm] [--stop-after N] [--dump]\n",
                argv[0]);
        return 1;
    }

    /* Check for --dump as second arg */
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dump") == 0) {
            dump_mode = 1;
        }
    }

    if (dump_mode) {
        return dump_recording(argv[1]);
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <recording.lba2polyrec> <output.raw> "
                "[--ppm output.ppm] [--ref-ppm ref.ppm] [--stop-after N] [--zbuf output.zbuf] [--dump]\n",
                argv[0]);
        return 1;
    }

    const char *zbuf_file = NULL;
    /* Parse optional arguments */
    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--ppm") == 0 && i + 1 < argc) {
            ppm_file = argv[++i];
        } else if (strcmp(argv[i], "--ref-ppm") == 0 && i + 1 < argc) {
            ref_ppm_file = argv[++i];
        } else if (strcmp(argv[i], "--stop-after") == 0 && i + 1 < argc) {
            stop_after = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--zbuf") == 0 && i + 1 < argc) {
            zbuf_file = argv[++i];
        }
    }

    int result = polyrec_replay_run(argv[1], argv[2], ppm_file, ref_ppm_file, stop_after);

    /* Write z-buffer if requested */
    if (zbuf_file && PtrZBuffer) {
        FILE *zf = fopen(zbuf_file, "wb");
        if (zf) {
            fwrite(PtrZBuffer, sizeof(U16), 640 * 480, zf);
            fclose(zf);
        }
    }

    return result;
}
