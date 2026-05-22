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
