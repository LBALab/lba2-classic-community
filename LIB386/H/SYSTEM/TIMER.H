#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/EVENTS.H> // For ManageEvents()

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
extern volatile U32 TimerSystemHR;
extern volatile U32 TimerRefHR;
extern volatile S32 CmptFrame;        ///< Frame count
extern volatile S32 NbFramePerSecond; ///< FPS - Frames per second
extern volatile S32 CmptMemoTimerRef; ///< Timer save/restore reference count

// --- Initialization ----------------------------------------------------------
void InitTimer();
void EndTimer();

// --- Interface ---------------------------------------------------------------
void LockTimer();
void UnlockTimer();
void SaveTimer();
void RestoreTimer();
void SetTimerHR(U32 time);

// --- Fixed-dt deterministic clock (harness-only) -----------------------------
// When enabled, ManageTime() advances from an internal virtual clock stepped by
// Timer_FixedDtAdvance() instead of SDL_GetTicks(), so the per-tick dt is constant
// and the simulation is independent of wall-clock. Off by default; the engine
// carries no default step (the caller supplies it). Default builds and any run that
// does not call Timer_EnableFixedDt() are byte-for-byte unaffected. See docs/CONTROL.md.
void Timer_EnableFixedDt(U32 dt_ms); // arm fixed-dt; seeds the virtual clock to now
void Timer_FixedDtAdvance();         // advance the virtual clock by dt_ms (once per tick)
S32 Timer_FixedDtActive();           // non-zero while fixed-dt is armed (guards wall-clock paths)
// Called once per presented frame (BoxBlit). The first present after each
// Timer_FixedDtAdvance() is free (it is the main-loop render the tick already paid
// for); every additional present in the same tick steps the virtual clock by dt_ms.
// This is how modal inner loops (DoFoundObj, messages, menus) — which present frames
// without going through the main-loop tick hook — keep the clock moving instead of
// deadlocking on a TimerRefHR deadline. Inert until the first tick advance, so boot
// presents do not move the clock. Non-fixed-dt runs are byte-for-byte unaffected.
void Timer_FixedDtPresent();
// Step the virtual clock by dt_ms for a non-presenting busy-wait. Some loops (the music
// volume fades in MUSIC.CPP) spin only on ManageTime() with no frame present, waiting for
// TimerRefHR to cross a deadline; under fixed-dt neither the tick hook nor a present fires
// inside them, so they would spin forever. Call this once per iteration of such a loop.
// Unlike Timer_FixedDtPresent() there is no free first step. Inert outside fixed-dt.
void Timer_FixedDtPump();

// --- Fixed simulation timestep (production, issue #358) ----------------------
// Movement/animation is only correct near ~60 fps because locomotion is baked into
// animation keyframes advanced once per rendered frame (see docs/MOVEMENT_FRAMERATE.md).
// This scheduler decouples the simulation rate from the render rate: given the real
// wall-clock elapsed since the last rendered frame, it returns how many fixed dt-sized
// simulation steps to run this frame, carrying the sub-step remainder so movement over
// game-time is invariant to the frame rate.
//
// Timer_FixedStepCount is the pure core (no clock, no globals): fold `elapsedMs` into
// `*accMs` and return the step count, clamped to `maxSteps` (a spiral-of-death guard;
// pass a negative value for no clamp). On clamp the backlog beyond the clamp is dropped,
// keeping only the sub-dt remainder. Post-condition: `*accMs < dt`. Unit-tested directly.
S32 Timer_FixedStepCount(U32 *accMs, U32 elapsedMs, U32 dt, S32 maxSteps);

// Plan one rendered frame of the production sub-step loop (SOURCES/PERSO.CPP MainLoop). Pure:
// no clock read, no globals. Given the frame's game clock `now` (TimerRefHR) and the game-time
// `lastSim` of the last simulated sub-step, return how many fixed `dt`-ms sub-steps to run this
// frame and, via the out-params, the anchor and the next `lastSim`:
//   *baseRefOut  -- sub-step k (0-based) runs at clock *baseRefOut + (k+1)*dt.
//   *lastSimOut  -- the caller writes this back to LastSimRefHR (carries the sub-dt remainder).
// Cases (the whole point of the fixed-timestep fix, issue #358):
//   0 steps, lastSim unchanged  -- rendering faster than the sim rate (elapsed < dt): skip and
//                                  re-present, the remainder carries in the lastSim gap.
//   0 steps, lastSim = now      -- the game clock rewound (a modal SaveTimer restore): re-anchor.
//   1 step,  grid re-anchored   -- elapsed > maxSteps*dt (first frame / load / cube jump / below
//                                  the catch-up floor): drop the backlog, step once at `now`.
//   N steps                     -- elapsed = N*dt (+remainder): sub-step so a long frame advances
//                                  the animation the correct amount instead of losing it.
// The normal-path count+carry delegates to Timer_FixedStepCount, so that tested core is the
// engine here. At dt == 16 and elapsed == 16 this returns exactly 1 step (the historical
// per-frame path, byte-for-byte). dt <= 0 (throttle off) returns 1 and leaves lastSim alone.
S32 Timer_PlanSimSteps(U32 now, U32 lastSim, S32 dt, S32 maxSteps, U32 *baseRefOut, U32 *lastSimOut);

// Promote a throttle-skip into a single delivered step when a one-frame game event is pending
// this frame (issue #358). `plannedSteps` is Timer_PlanSimSteps' result; `pending` is non-zero
// when the caller has a discrete event that the object loop must consume this frame (an inventory
// item used this frame, consumed by LF_USE_INVENTORY in DoLife). If the frame already runs a step
// (plannedSteps > 0) or nothing is pending, the plan is returned untouched. Otherwise the skip is
// promoted to one step at `now`: the sub-step grid is re-anchored to `now - dt` (so the step runs
// at `now`) and `*lastSimOut` to `now`, so the event is delivered without the throttle swallowing
// it on high-refresh displays. Pure and unit-tested; the main loop's event-delivery guard lives
// here rather than in the throttle math. See SOURCES/PERSO.CPP and docs/MOVEMENT_FRAMERATE.md.
S32 Timer_ForceStepIfPending(S32 plannedSteps, S32 pending, U32 now, S32 dt, U32 *baseRefOut,
                             U32 *lastSimOut);

void ManageTime();
void HandleEventsTimer(const void *event);

// -----------------------------------------------------------------------------
#define ManageSystem()  \
    {                   \
        ManageEvents(); \
        ManageTime();   \
    }

// =============================================================================
#ifdef __cplusplus
}
#endif
