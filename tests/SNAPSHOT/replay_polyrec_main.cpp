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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *ppm_file = NULL;
    const char *ref_ppm_file = NULL;
    int stop_after = -1;  /* -1 = replay all */
    int i;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <recording.lba2polyrec> <output.raw> "
                "[--ppm output.ppm] [--ref-ppm ref.ppm] [--stop-after N]\n",
                argv[0]);
        return 1;
    }

    /* Parse optional arguments */
    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--ppm") == 0 && i + 1 < argc) {
            ppm_file = argv[++i];
        } else if (strcmp(argv[i], "--ref-ppm") == 0 && i + 1 < argc) {
            ref_ppm_file = argv[++i];
        } else if (strcmp(argv[i], "--stop-after") == 0 && i + 1 < argc) {
            stop_after = atoi(argv[++i]);
        }
    }

    return polyrec_replay_run(argv[1], argv[2], ppm_file, ref_ppm_file, stop_after);
}
