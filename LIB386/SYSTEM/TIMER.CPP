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
U32 LastSimRefHR = 0; // #358 sub-step carry; see TIMER.H

// --- Private state -----------------------------------------------------------
volatile S32 TimerLock;
volatile S32 CmptMemoTimerRef;
static U32 MemoTimerRefHR;
U32 LastTime;
U32 LastEvaluate;

// FPS re-evaluation window (ms). The on-screen counter (AffNbFps) and the
// --dump-state "fps" field both read NbFramePerSecond, which is recomputed
// once per window. The original 1000 ms made the displayed number sit still
// for a full second then jump, reading as "frozen"; 250 ms updates it ~4x/s so
// it tracks performance changes (e.g. moving between cheap interior and the
// render-bound exterior terrain) without being frame-to-frame jittery. The
// result is still normalised to frames-per-second, so the value's meaning is
// unchanged.
#define FPS_EVAL_WINDOW_MS 250

// How far behind real time the paced clock may fall before it stops trying to catch up.
#define PACE_GIVEUP_MS 1000

// Fixed-dt deterministic clock (harness-only). Inert unless Timer_EnableFixedDt() is
// called: while active, ManageTime() reads FixedDtNow instead of SDL_GetTicks().
static S32 FixedDtActive = 0;
static U32 FixedDt = 0;
static U32 FixedDtNow = 0;            // minted only by FixedDtStep(); seeded by Timer_EnableFixedDt
static S32 FixedDtTicking = 0;        // set on the first tick advance; gates present-stepping past boot
static S32 FixedDtSkipPresent = 0;    // makes the first present after each tick advance free
static S32 FixedDtOverlayPresent = 0; // the next present draws an overlay, not a frame
static S32 FixedDtPaced = 0;          // spend real time per step; see Timer_SetFixedDtPaced
static U32 PaceAnchor = 0;            // wall clock when pacing was armed or last re-anchored
static U32 PaceSteps = 0;             // steps minted since then
static S32 SimAudioClock = 0;         // see Timer_SimAudioClock (TIMER.H)

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
    // arming fixed-dt, and that residual would otherwise reach Rnd_Seed(TimerRefHR) at
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
    FixedDtPaced = 0;
}

void Timer_DisableFixedDt(void) {
    if (!FixedDtActive)
        return;
    FixedDtActive = 0;
    FixedDtTicking = 0;
    FixedDtSkipPresent = 0;
    FixedDtOverlayPresent = 0;
    FixedDtPaced = 0;
    // Resync LastTime to the wall clock before any of it can be banked. The two clocks
    // have been running at different rates since the arm -- the virtual one moves dt per
    // frame, the wall one moves in wall time -- so by now they are far apart, and the
    // first ManageTime() after this would bank the whole difference into TimerRefHR:
    // forwards if the wall clock is ahead, and much further if it is behind, since the
    // subtraction is unsigned. Re-anchor the FPS window for the same reason.
    LastTime = SDL_GetTicks();
    LastEvaluate = LastTime;
    CmptFrame = 0;
}

/* The expression ManageTime reads, evaluated now. Recording SDL_GetTicks() directly is
   wrong under fixed-dt, where the game runs on the virtual clock and the wall clock is
   unrelated to it. */
U32 Timer_ClockSource(void) {
    return FixedDtActive ? FixedDtNow : SDL_GetTicks();
}

S32 Timer_FixedDtValue(void) {
    return FixedDtActive ? (S32)FixedDt : 0;
}

/* Sleep until the wall clock reaches `target`, and answer how far ahead of it this call
   was. Shared by everything that mints a step of clock and then has to pay for it, so the
   arithmetic below is written once.

   That arithmetic is a signed difference of two unsigned readings, not a comparison of
   their magnitudes. Both wrap -- the host clock every 49.7 days, and any anchor plus a
   step count with it -- and either one wrapping first makes a magnitude test read the gap
   as nearly 2^32 ms, so the sleep would be weeks rather than milliseconds. The subtraction
   wraps to the true small difference and survives it. */
S32 Timer_SleepUntil(U32 target) {
    S32 ahead = (S32)(target - (U32)SDL_GetTicks());

    if (ahead > 0)
        SDL_Delay((U32)ahead);
    return ahead;
}

/* Mint one step of virtual time, and -- when something is holding the clock to real time
   -- spend a step of real time on it.

   Every dt the pinned clock hands out passes through here, from all three sites that
   hand one out: the tick advance, an extra present, and a non-presenting pump. That is
   the whole point of the funnel. Pacing per tick instead covers the main loop and misses
   the other two, which are exactly the modal ones: a fade, a menu or a dialogue box runs
   its own loop that presents and pumps without ever reaching the main-loop tick, so it
   would draw as much virtual time as it liked at no cost in wall time. Measured on the
   FadeToBlack path (SOURCES/AMBIANCE.CPP), which waits out FADE_DELAY of game time:
   208 ms of clock in 26 ms of wall, eight times real speed, inside a run whose overall
   rate read 1.02x because the paced main loop averaged the spike away.

   The target is computed from the anchor and the step count rather than from the last
   step, so the error does not accumulate: a step that overruns is paid for by the next
   one arriving early rather than by every later step sliding. */
static void FixedDtStep(void) {
    S32 ahead;

    FixedDtNow += FixedDt;

    if (!FixedDtPaced)
        return;

    PaceSteps++;
    ahead = Timer_SleepUntil(PaceAnchor + PaceSteps * FixedDt);
    if (ahead < -PACE_GIVEUP_MS) {
        /* Too far behind to catch up. Give the time up rather than sprint to win it
           back, which would run the game fast to make up for having run it slow. The
           ordinary way to reach this is a window that lost focus: the clock stops, the
           wall clock does not, and the run resumes owing more time than it can repay. */
        PaceAnchor = (U32)SDL_GetTicks();
        PaceSteps = 0;
    }
}

void Timer_FixedDtAdvance() {
    // One fixed step per tick. The next unlocked ManageTime() banks it into TimerRefHR;
    // every other ManageTime() in the tick sees an unchanged FixedDtNow and adds nothing.
    if (FixedDtActive) {
        FixedDtStep();
        FixedDtTicking = 1;
        // The main-loop render that follows this advance presents one frame; let it pass
        // for free so a normal tick stays exactly +dt. Extra presents in this tick come
        // from modal inner loops and each step the clock (see Timer_FixedDtPresent).
        FixedDtSkipPresent = 1;
    }
}

void Timer_FixedDtOverlayPresent() { FixedDtOverlayPresent = 1; }

void Timer_FixedDtPresent() {
    /* An overlay refresh is not a frame the simulation asked for, so it neither steps the
       clock nor spends the tick's free present. Spending it would be as wrong as stepping:
       the main-loop render still follows, and it would pay the step the overlay dodged. */
    if (FixedDtOverlayPresent) {
        FixedDtOverlayPresent = 0;
        return;
    }
    if (FixedDtActive && FixedDtTicking) {
        if (FixedDtSkipPresent) {
            FixedDtSkipPresent = 0;
        } else {
            FixedDtStep();
        }
    }
}

/* Session recording: a loop that waits on the clock without polling input gets no new
   reading from the clock hook below, because there is no poll to take one at. Weak, so
   libsys.a still links without the recorder. */
void Record_WaitHook(void);
__attribute__((weak)) void Record_WaitHook(void) {}

/* A wait that ends on the clock and does not poll input. The caller has nothing to do but
   let time pass, so whatever is minting time for this run has to mint it here: the step
   below under --fixed-dt, and the wait hook under a clock the recorder is holding. A run
   with neither reads the host clock directly and needs nothing from this. */
void Timer_FixedDtPump() {
    // No free first step: a non-presenting wait loop owns every iteration's time.
    if (FixedDtActive && FixedDtTicking) {
        FixedDtStep();
    }
    Record_WaitHook();
}

S32 Timer_FixedDtActive() {
    return FixedDtActive;
}

void Timer_SetFixedDtPaced(S32 on) {
    FixedDtPaced = on ? 1 : 0;
    /* Anchor on the arm, not on the first step: the caller arms this at the moment it
       wants real time to start counting from. */
    PaceAnchor = SDL_GetTicks();
    PaceSteps = 0;
}

void Timer_SetSimAudioClock(S32 on) {
    SimAudioClock = on ? 1 : 0;
}

S32 Timer_SimAudioClock(void) {
    return FixedDtActive || SimAudioClock;
}

S32 Timer_FixedStepCount(U32 *accMs, U32 elapsedMs, U32 dt, S32 maxSteps) {
    // Pure accumulator (issue #358). No clock read, no globals: fold the elapsed
    // real time into the carry and hand back how many fixed dt-steps of simulation
    // are due. The remainder stays in *accMs, so over any span of wall-time the total
    // step count is span/dt regardless of how the elapsed time was chunked -- that is
    // what makes movement frame-rate independent. See docs/MOVEMENT_FRAMERATE.md.
    if (dt == 0) {
        return 0; // guard: a zero step would divide by zero and never advance
    }

    *accMs += elapsedMs;

    S32 steps = (S32)(*accMs / dt);

    if (maxSteps >= 0 && steps > maxSteps) {
        // Behind by more than we will run this frame (a long stall, or a modal that
        // banked real time while the game clock was frozen). Run the clamp and drop
        // the backlog so we do not spiral trying to catch up; keep the sub-dt remainder.
        steps = maxSteps;
        *accMs %= dt;
    } else {
        *accMs -= (U32)steps * dt;
    }

    return steps;
}

S32 Timer_PlanSimSteps(U32 now, U32 lastSim, S32 dt, S32 maxSteps, U32 *baseRefOut, U32 *lastSimOut) {
    // Default: a single step anchored at the frame start (the throttle-off / per-frame path).
    *baseRefOut = lastSim;
    *lastSimOut = lastSim;
    if (dt <= 0) {
        return 1;
    }

    // Signed so a backward game-clock jump (a modal SaveTimer restore, a cube change) reads
    // negative rather than wrapping to a huge positive gap.
    S32 elapsed = (S32)(now - lastSim);
    if (elapsed < 0) {
        *lastSimOut = now; // rewind: re-anchor the step grid, run no sub-step this frame
        return 0;
    }
    if (elapsed > maxSteps * dt) {
        // First frame, a load/cube clock jump, or below the catch-up floor: drop the backlog
        // and run a single step at the current clock (grid re-anchored to now - dt).
        *baseRefOut = now - (U32)dt;
        *lastSimOut = now;
        return 1;
    }

    // Normal path. Fold the whole elapsed span through the tested accumulator: with acc seeded
    // to 0 it returns elapsed/dt and leaves the sub-dt remainder in acc (elapsed <= maxSteps*dt
    // here, so the clamp never fires -- the large-jump guard above owns that). lastSim advances
    // by nSteps*dt == now - remainder, which carries the remainder into the next frame.
    U32 acc = 0;
    S32 nSteps = Timer_FixedStepCount(&acc, (U32)elapsed, (U32)dt, -1);
    if (nSteps <= 0) {
        return 0; // rendering faster than the sim rate: skip, remainder carries in the gap
    }
    *lastSimOut = now - acc;
    return nSteps;
}

S32 Timer_ForceStepIfPending(S32 plannedSteps, S32 pending, U32 now, S32 dt, U32 *baseRefOut,
                             U32 *lastSimOut) {
    // A frame that already runs a step consumes the pending event then; nothing pending means the
    // throttle stands (keeps the high-fps movement decoupling untouched). Only a would-skip frame
    // with a pending event needs promoting.
    if (plannedSteps > 0 || !pending) {
        return plannedSteps;
    }

    // Promote the skip to a single step at `now`: anchor the sub-step grid to now - dt (so the one
    // step runs at `now`, like a normal ~60 Hz frame) and lastSim to `now`, re-anchoring the grid
    // so the next frame throttles cleanly from here. This is what stops the fixed-timestep throttle
    // from swallowing a one-frame inventory-use on high-refresh displays (issue #358).
    *baseRefOut = now - (U32)dt;
    *lastSimOut = now;
    return 1;
}

/* Session recording: the clock is captured where it is read, not where it
   is convenient. Sampling it at the input poll and replaying that is close but never
   exact, and pinning it at record time makes the recorder change the run it is meant
   to observe. Weak, so libsys.a still links without the recorder. */
void Record_ClockHook(U32 *now);
__attribute__((weak)) void Record_ClockHook(U32 *now) { (void)now; }

void ManageTime() {
    U32 nowSample = FixedDtActive ? FixedDtNow : SDL_GetTicks();
    Record_ClockHook(&nowSample);
    TimerSystemHR = nowSample;

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
    S32 time = (S32)(TimerSystemHR - LastEvaluate);
    if (time < 0) {
        // The clock origin moved backwards relative to the FPS window anchor
        // (e.g. the fixed-dt harness reseeding TimerSystemHR in
        // Timer_EnableFixedDt). A negative delta never reaches the window test
        // below, which would freeze NbFramePerSecond permanently. Re-anchor
        // instead of going dormant.
        LastEvaluate = TimerSystemHR;
        CmptFrame = 0;
    } else if (time >= FPS_EVAL_WINDOW_MS) {
        NbFramePerSecond = (CmptFrame * 1000 + time / 2) / time;
        CmptFrame = 0;
        LastEvaluate += (time / FPS_EVAL_WINDOW_MS) * FPS_EVAL_WINDOW_MS;
    }
}

/* Set by the control harness: a batch run must keep ticking even when the window is
   not focused. Pausing on focus loss is right for a player (see HandleEventsTimer) but
   it silently makes an automated run nondeterministic, because whether the clock stops
   depends on what else the desktop is doing. */
static S32 TimerIgnoreFocus = 0;

void Timer_SetIgnoreFocus(S32 ignore) {
    TimerIgnoreFocus = ignore ? 1 : 0;
}

void HandleEventsTimer(const void *event) {
    assert(event != NULL);

    if (TimerIgnoreFocus) {
        return;
    }

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
