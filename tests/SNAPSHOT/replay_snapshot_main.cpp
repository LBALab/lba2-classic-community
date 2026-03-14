/*
 * replay_snapshot_main.cpp — Main entry point for snapshot replay programs
 *
 * This file is compiled into two executables:
 *   - replay_snapshot_asm  (linked against ASM library)
 *   - replay_snapshot_cpp  (linked against CPP library)
 *
 * Usage: replay_snapshot_asm <snapshot.lba2snap> <output.raw> [--ppm output.ppm] [--ref-ppm ref.ppm] [--only-object N]
 *        replay_snapshot_cpp <snapshot.lba2snap> <output.raw> [--ppm output.ppm] [--ref-ppm ref.ppm] [--only-object N]
 */

#include "snapshot_replay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *ppm_file = NULL;
    const char *ref_ppm_file = NULL;
    int only_object = -1;  /* -1 = render all */
    int i;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <snapshot.lba2snap> <output.raw> [--ppm output.ppm] [--ref-ppm ref.ppm] [--only-object N]\n", argv[0]);
        return 1;
    }

    /* Parse optional arguments */
    for (i = 3; i < argc - 1; i++) {
        if (strcmp(argv[i], "--ppm") == 0) {
            ppm_file = argv[i + 1];
        } else if (strcmp(argv[i], "--ref-ppm") == 0) {
            ref_ppm_file = argv[i + 1];
        } else if (strcmp(argv[i], "--only-object") == 0) {
            only_object = atoi(argv[i + 1]);
        }
    }

    return snapshot_replay_run(argv[1], argv[2], ppm_file, ref_ppm_file, only_object);
}
