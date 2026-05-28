/* Host-only regression test for the TIMER.CPP LastTime-outside-lock bug
 * (PR #252 / commit ed0e7a64): ManageTime() must NOT advance LastTime
 * while TimerLock is held.
 *
 * The original 1997 source (and the SDL port through 2026-05) advanced
 * LastTime unconditionally, which silently discarded wall-clock elapsed
 * during any Lock window from TimerRefHR. Sub-millisecond on bare-metal
 * DOS where the locked window was synchronous CPU work; catastrophic under
 * SDL3 where the locked window holds a full vsync wait (~16.67 ms at 60 Hz)
 * inside the AffScene full-redraw path.
 *
 * If this test fails, the LastTime semantic has regressed. See
 * docs/TIMING.md for the full history and `LIB386/SYSTEM/TIMER.CPP`
 * ManageTime() for the fix.
 *
 * Test strategy: drive the timer in fixed-dt mode (Timer_EnableFixedDt)
 * so the wall clock is virtual and fully under our control. No SDL ticks,
 * no sleeps, deterministic across hosts.
 */

#include <SYSTEM/TIMER.H>

#include <cassert>
#include <cstdio>

/* Stub for WINDOW.H AppActive (referenced by TIMER.CPP::HandleEventsTimer).
 * This test never calls HandleEventsTimer, but the symbol must resolve for
 * link. */
bool AppActive = true;

static int fails = 0;

static void check_eq(U32 got, U32 want, const char *label) {
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s: TimerRefHR=%u (want %u)\n", label, got, want);
        fails++;
    }
}

int main() {
    /* (1) Lock window must be transparent to TimerRefHR.
     *
     * Advance the (virtual) clock outside the lock, then inside the lock --
     * every wall-clock ms elapsed must be credited to TimerRefHR by the
     * first unlocked ManageTime() after UnlockTimer() releases. This is the
     * direct regression test for the original bug. */
    {
        Timer_EnableFixedDt(16);
        ManageTime(); /* bootstrap LastTime to FixedDtNow=0 */
        check_eq(TimerRefHR, 0, "(1) post-init");

        Timer_FixedDtAdvance(); /* FixedDtNow -> 16 */
        ManageTime();           /* unlocked: credits +16 */
        check_eq(TimerRefHR, 16, "(1) one tick before lock");

        LockTimer(); /* calls ManageTime then ++TimerLock */

        Timer_FixedDtAdvance(); /* FixedDtNow -> 32 */
        ManageTime();           /* locked: must NOT bump LastTime */
        Timer_FixedDtAdvance(); /* FixedDtNow -> 48 */
        ManageTime();           /* locked: must NOT bump LastTime */

        UnlockTimer(); /* calls ManageTime (still locked), then --TimerLock */
        ManageTime();  /* unlocked: credits the whole locked window */

        /* Pre-fix: bug discarded the 32 ms locked window. TimerRefHR stuck
         * at 16. Post-fix: full wall-clock advance credited. TimerRefHR=48. */
        check_eq(TimerRefHR, 48, "(1) post-unlock (the regression-pin assertion)");
    }

    /* (2) SaveTimer/RestoreTimer DOES discard elapsed time -- documented
     * semantic difference from LockTimer (docs/TIMING.md "Lock vs Save").
     * This pins the intentional rewind so a future contributor cleaning up
     * Save/Restore doesn't accidentally turn it into a transparent pause. */
    {
        Timer_EnableFixedDt(16);
        ManageTime();
        Timer_FixedDtAdvance();
        ManageTime();
        check_eq(TimerRefHR, 16, "(2) one tick before save");

        SaveTimer(); /* snapshots TimerRefHR=16 */
        Timer_FixedDtAdvance();
        ManageTime(); /* SaveTimer doesn't hold TimerLock, so this credits */
        Timer_FixedDtAdvance();
        ManageTime();
        RestoreTimer(); /* TimerRefHR = snapshot, discards 32 ms */
        ManageTime();

        /* Post-restore: snapshot restored, two ticks of game time gone. */
        check_eq(TimerRefHR, 16, "(2) post-restore (rewind semantic)");
    }

    /* (3) Nested LockTimer balances correctly: the locked window covers
     * all nested calls and TimerRefHR catches up on the outermost release. */
    {
        Timer_EnableFixedDt(16);
        ManageTime();
        Timer_FixedDtAdvance();
        ManageTime();
        check_eq(TimerRefHR, 16, "(3) one tick before nested lock");

        LockTimer(); /* depth 0 -> 1 */
        Timer_FixedDtAdvance();
        LockTimer(); /* nested: depth 1 -> 2 */
        Timer_FixedDtAdvance();
        ManageTime();  /* still locked */
        UnlockTimer(); /* depth 2 -> 1 (still locked) */
        Timer_FixedDtAdvance();
        ManageTime();  /* still locked */
        UnlockTimer(); /* depth 1 -> 0 */
        ManageTime();  /* unlocked: credits the whole window */

        /* 3 advances * 16ms = 48 ms of locked wall clock, all credited. */
        check_eq(TimerRefHR, 64, "(3) post-nested-unlock");
    }

    assert(fails == 0);
    std::printf("test_timer_lock_window: ok\n");
    return 0;
}
