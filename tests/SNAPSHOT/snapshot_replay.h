/*
 * snapshot_replay.h — Shared header for snapshot replay programs
 *
 * Used by both replay_snapshot_asm and replay_snapshot_cpp.
 */

#pragma once

#include <SNAPSHOT/SNAPSHOT.H>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a snapshot, apply all rendering globals, allocate framebuffer + Z-buffer,
 * render all objects, and write the raw framebuffer to output_file.
 *
 * @param snapshot_file  Path to .lba2snap file
 * @param output_file    Path to write raw framebuffer output
 * @return 0 on success, non-zero on failure
 */
int snapshot_replay_run(const char *snapshot_file, const char *output_file);

#ifdef __cplusplus
}
#endif
