/*
 * replay_snapshot_main.cpp — Main entry point for snapshot replay programs
 *
 * This file is compiled into two executables:
 *   - replay_snapshot_asm  (linked against ASM library)
 *   - replay_snapshot_cpp  (linked against CPP library)
 *
 * Usage: replay_snapshot_asm <snapshot.lba2snap> <output.raw>
 *        replay_snapshot_cpp <snapshot.lba2snap> <output.raw>
 */

#include "snapshot_replay.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <snapshot.lba2snap> <output.raw>\n", argv[0]);
        return 1;
    }

    return snapshot_replay_run(argv[1], argv[2]);
}
