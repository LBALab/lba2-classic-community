#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// Render-interpolation core (issue #412, docs/RENDER_INTERP_PLAN.md). Pure: no globals, no clock
// read. The fixed-timestep sim (#358) advances at ~60 Hz; on a faster display the extra rendered
// frames re-present a frozen pose (judder). These primitives let the render draw each object between
// its last two sim-step positions, so motion is smooth every frame while the sim stays byte-exact.

// Interpolate a world coordinate between the previous and current sim-step values by the fraction
// num/den. num==0 returns prev exactly; num>=den (or den==0) returns cur exactly. The endpoints are
// byte-exact, so a frame that lands on a step boundary matches the un-interpolated position and the
// projection golden is unchanged. A 64-bit intermediate keeps large spans from overflowing.
S32 Render_LerpCoord(S32 prev, S32 cur, U32 num, U32 den);

// Banked game-time since the last sim step, as the numerator of the interpolation fraction over dt
// (the step size). Returns a value in [0, dt]: 0 right after a step, or when dt<=0 (interpolation
// off), or when the clock ran backwards (never expected); dt at or past the next step boundary, which
// makes Render_LerpCoord return the current position. Pair with Render_LerpCoord(prev, cur, num, dt).
U32 Render_InterpNum(U32 now, U32 lastSim, S32 dt);

// -----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif
