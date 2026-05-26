/*
 * Runtime resolution switching — orchestrator.
 *
 * See docs/RUNTIME_RESOLUTION.md for the design.  Res_Switch tears down the
 * framebuffer-sized buffers and the SDL surface, reinitialises both at the new
 * dimensions, and reloads the current cube's grille data (Bricks / BlockTable /
 * MaskBrick) so the next AffScene() can present a complete frame.  On any
 * rebuild failure the orchestrator falls back to the previous (W, H).
 *
 * This is the engine-side primitive; the console verb and the keep / revert
 * dialog described in the doc are user-facing layers that drive it.
 */

#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns true on success, false if newW/newH is outside the engine's
   supported range, isn't a multiple of 8 in width, or if the rebuild failed
   and the previous resolution had to be restored. */
bool Res_Switch(U32 newW, U32 newH);

#ifdef __cplusplus
}
#endif
