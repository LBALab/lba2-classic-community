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
    if (FixedDtActive && FixedDtTicking) {
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
        // Only advance LastTime while unlocked. Previously LastTime advanced
        // unconditionally, so any ManageTime() call inside a Lock/Save window
        // (input, audio, animation -- ~100 call sites) would update LastTime
        // to the current wall clock while skipping the TimerRefHR increment.
        // On the next unlocked call, TimerSystemHR - LastTime was near zero
        // and the entire locked interval was discarded from the game clock.
        // PR #50 patched this for the FollowCamTerrainFrame path (PERSO.CPP);
        // this generalises the fix to every Lock/Save call site.
        LastTime = TimerSystemHR;
    }

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
