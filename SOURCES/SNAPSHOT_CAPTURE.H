/*
 * SNAPSHOT_CAPTURE.H — Scene Snapshot Capture (game-side)
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Capture the current scene rendering state and write it to a file.
 * Call this at the start of AffScene() after camera/lights are set up.
 * @param filename  Output file path (.lba2snap)
 * @return 0 on success, -1 on failure
 */
int Snapshot_CaptureScene(const char *filename);

/**
 * Append the game's rendered framebuffer to an existing snapshot file.
 * Call this AFTER AffScene() completes to capture the full rendered frame
 * (background + 3D objects) as a reference image.
 * @param filename  Path to .lba2snap file (must already exist)
 * @return 0 on success, -1 on failure
 */
int Snapshot_CaptureGameFramebuffer(const char *filename);

#ifdef __cplusplus
}
#endif
