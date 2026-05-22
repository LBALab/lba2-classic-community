#include <SYSTEM/TIMER.H>

#include <SYSTEM/WINDOW.H> // For AppActive

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>
#include <assert.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
volatile U32 TimerSystemHR;
volatile U32 TimerRefHR;
volatile S32 CmptFrame;
volatile S32 NbFramePerSecond;

// --- Private state -----------------------------------------------------------
volatile S32 TimerLock;
volatile S32 CmptMemoTimerRef;
static U32 MemoTimerRefHR;
U32 LastTime;
U32 LastEvaluate;

// Fixed-dt deterministic clock (harness-only). Inert unless Timer_EnableFixedDt() is
// called: while active, ManageTime() reads FixedDtNow instead of SDL_GetTicks().
static S32 FixedDtActive = 0;
static U32 FixedDt = 0;
static U32 FixedDtNow = 0;

// --- Initialization ----------------------------------------------------------
void InitTimer() {
    LastTime = LastEvaluate = SDL_GetTicks();
    ManageTime();
}

void EndTimer() {
    // Empty
}

// -----------------------------------------------------------------------------
void LockTimer() {
    ManageTime();
    ++TimerLock;
}

void UnlockTimer() {
    if (TimerLock > 0) {
        ManageTime();
        --TimerLock;
    }
}

void SaveTimer() {
    if (!CmptMemoTimerRef++) {
        ManageTime();
        MemoTimerRefHR = TimerRefHR;
    }
}

void RestoreTimer() {
    if (CmptMemoTimerRef) {
        if (!--CmptMemoTimerRef) {
            ManageTime();
            TimerRefHR = MemoTimerRefHR;
        }
    }
}

void SetTimerHR(U32 time) {
    LockTimer();
    TimerRefHR = time;
    UnlockTimer();
}

void Timer_EnableFixedDt(U32 dt_ms) {
    // Seed the virtual clock and force LastTime to agree with it, so the first
    // ManageTime() delta (FixedDtNow - LastTime) is exactly 0 and no residual
    // wall-clock interval banked during boot/load leaks into the first tick. From here
    // the clock only moves via Timer_FixedDtAdvance(). Keeps the FPS accumulator sane.
    FixedDt = dt_ms;
    FixedDtNow = TimerSystemHR;
    LastTime = TimerSystemHR;
    FixedDtActive = 1;
}

void Timer_FixedDtAdvance() {
    // One fixed step per tick. The next unlocked ManageTime() banks it into TimerRefHR;
    // every other ManageTime() in the tick sees an unchanged FixedDtNow and adds nothing.
    if (FixedDtActive) {
        FixedDtNow += FixedDt;
    }
}

S32 Timer_FixedDtActive() {
    return FixedDtActive;
}

void ManageTime() {
    TimerSystemHR = FixedDtActive ? FixedDtNow : SDL_GetTicks();

    if (!TimerLock) {
        TimerRefHR += TimerSystemHR - LastTime;
    }

    LastTime = TimerSystemHR;

    // FPS Calculation
    S32 time = TimerSystemHR - LastEvaluate;
    if (time >= 1000) {
        NbFramePerSecond = (CmptFrame * 1000 + time / 2) / time;
        CmptFrame = 0;
        LastEvaluate += (time / 1000) * 1000;
    }
}

void HandleEventsTimer(const void *event) {
    assert(event != NULL);

    const SDL_Event *sdlEvent = (SDL_Event *)(event);
    switch (sdlEvent->type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (!AppActive) {
            UnlockTimer();
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (AppActive) {
            LockTimer();
        }
        break;
    default:
        break;
    }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
