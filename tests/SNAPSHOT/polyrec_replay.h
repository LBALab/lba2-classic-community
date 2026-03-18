/*
 * polyrec_replay.h — Shared header for polygon recording replay programs
 *
 * Used by both replay_polyrec_asm and replay_polyrec_cpp.
 */

#pragma once

#include <SNAPSHOT/POLY_RECORDING.H>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a polygon recording, apply initial state, allocate framebuffer + Z-buffer,
 * replay all polygon calls, and write the raw framebuffer to output_file.
 *
 * @param polyrec_file  Path to .lba2polyrec file
 * @param output_file   Path to write raw framebuffer
 * @param ppm_file      If non-NULL, write PPM color image (requires RGB palette)
 * @param ref_ppm_file  If non-NULL, write reference game framebuffer as PPM
 * @param start_after   If >= 0, skip executing draw calls up to and including this
 *                      number while still applying non-draw state changes
 * @param stop_after    If >= 0, stop after this many draw calls (for bisection or
 *                      targeted replay windows)
 * @return 0 on success, nonzero on failure
 */
int polyrec_replay_run(const char *polyrec_file, const char *output_file,
                       const char *ppm_file, const char *ref_ppm_file,
                       int start_after, int stop_after);

#ifdef __cplusplus
}
#endif
