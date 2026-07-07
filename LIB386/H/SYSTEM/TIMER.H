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
