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
static S32 FixedDtTicking = 0;     // set on the first tick advance; gates present-stepping past boot
static S32 FixedDtSkipPresent = 0; // makes the first present after each tick advance free

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
    // wall-clock interval banked during boot/load leaks into the first tick. Also reset
    // TimerRefHR -- the existing accumulated value before this call carries whatever
    // wall-clock interval elapsed between InitTimer() at engine boot and the harness
    // arming fixed-dt, and that residual would otherwise reach srand(TimerRefHR) at
    // ChangeCube (OBJECT.CPP) and seed RNG-driven wanderers differently each run on the
    // fresh-start path (--demo without --load). The --load path overrides TimerRefHR from
    // the save after this, so save-loaded runs are unaffected. From here the clock only
    // moves via Timer_FixedDtAdvance(). Keeps the FPS accumulator sane.
    FixedDt = dt_ms;
    FixedDtNow = TimerSystemHR;
    LastTime = TimerSystemHR;
    TimerRefHR = 0;
    FixedDtActive = 1;
    FixedDtTicking = 0;
    FixedDtSkipPresent = 0;
}

void Timer_DisableFixedDt() {
    // Symmetric to Timer_EnableFixedDt: stop banking virtual time and re-anchor LastTime
    // to wall clock so the next ManageTime() banks a 0 delta into TimerRefHR (no jump from
    // the just-frozen virtual clock back to SDL_GetTicks). Used when the attract reel exits
    // back to interactive play -- harness --exit paths never call this since the process
    // is going away anyway. See issue #176 (interactive SHIFT+D needs fixed-dt for canonical
    // pacing; this restores real-time pacing when the user returns to the menu).
    FixedDtActive = 0;
    FixedDtTicking = 0;
    FixedDtSkipPresent = 0;
    LastTime = SDL_GetTicks();
}

void Timer_FixedDtAdvance() {
    // One fixed step per tick. The next unlocked ManageTime() banks it into TimerRefHR;
    // every other ManageTime() in the tick sees an unchanged FixedDtNow and adds nothing.
    if (FixedDtActive) {
        FixedDtNow += FixedDt;
        FixedDtTicking = 1;
        // The main-loop render that follows this advance presents one frame; let it pass
        // for free so a normal tick stays exactly +dt. Extra presents in this tick come
        // from modal inner loops and each step the clock (see Timer_FixedDtPresent).
        FixedDtSkipPresent = 1;
    }
}

void Timer_FixedDtPresent() {
    if (FixedDtActive && FixedDtTicking) {
        if (FixedDtSkipPresent) {
            FixedDtSkipPresent = 0;
        } else {
            FixedDtNow += FixedDt;
        }
    }
}

void Timer_FixedDtPump() {
    // No free first step: a non-presenting wait loop owns every iteration's time.
    // No FixedDtTicking gate either: Pump callers (FadeToBlack/WhiteFade/FadeWhiteToPal,
    // music volume fades, Res_Switch revert dialog) can run before MainLoop has its first
    // Timer_FixedDtAdvance() -- specifically InitGame(1)'s FadeToBlack between
    // BeginDemoSlide() and the first MainLoop tick on the interactive SHIFT+D path. With
    // the gate, those waits hang because the clock can't move and the wait condition (a
    // TimerRefHR/SystemHR deadline) never resolves; without it the clock advances exactly
    // dt per Pump as intended. Harness paths reach Pump only after MainLoop is ticking, so
    // they are unaffected.
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
