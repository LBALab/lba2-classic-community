#ifndef DEMO_SEED_H
#define DEMO_SEED_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Selects the value passed to srand() inside ChangeCube (SOURCES/OBJECT.CPP).
 *
 * Retail engine (and our default play): srand(TimerRefHR). The seed is a wall-
 * clock interval, which is fine in interactive play (any RNG state is fine —
 * humans don't notice).
 *
 * Demo / attract reel (DemoSlide == TRUE): srand((U32)new_cube). The wall-clock
 * branch made every cube entry in the reel reseed from "ms accumulated since
 * the last ManageTime that updated TimerRefHR", which varied by host speed —
 * the visible symptom was the Desert Island bat hitting Twinsen on macOS but
 * not on Linux, because the reel's RNG-driven wanderers (the bat included)
 * branched on a host-speed-derived seed. PR #249 tried to pin the seed by
 * zeroing TimerRefHR in BeginDemoSlide(), but it only worked under
 * --fixed-dt: in real-time mode, ManageTime() inside FadeToBlack/SaveTimer
 * runs between BeginDemoSlide() and the first ChangeCube and banks
 * (SDL_GetTicks() - LastTime) right back into TimerRefHR. And every later
 * ChangeCube during the reel reseeds from a still-accumulating clock.
 *
 * Seeding from the cube number makes the demo deterministic on every host
 * regardless of wall-clock pace: the same cube always starts with the same
 * RNG state. See issue #176 for the diagnosis. */
extern U32 Demo_RngSeed(S32 demo_slide, U32 timer_ref_hr, S32 new_cube);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_SEED_H */
