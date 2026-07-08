/* Fixed simulation-timestep scheduler (issue #358).
 *
 * Timer_FixedStepCount is the pure core of the fixed-timestep fix: fold the real
 * wall-clock elapsed since the last rendered frame into a carry, and return how many
 * fixed dt-sized simulation steps are due. The property that makes movement frame-rate
 * independent is that over any span of wall-time the TOTAL number of steps is span/dt,
 * no matter how the elapsed time was chunked across frames. These tests assert exactly
 * that, plus the spiral-of-death clamp and the sub-dt remainder carry.
 *
 * CPP-only host test; compiles the real TIMER.CPP. See docs/MOVEMENT_FRAMERATE.md.
 */
#include "../test_harness.h"
#include <SYSTEM/TIMER.H>

/* Stub for WINDOW.H AppActive (referenced by TIMER.CPP::HandleEventsTimer, which the
 * whole-file compile pulls in even though this test only exercises the scheduler). */
bool AppActive = true;

/* Feed a per-frame elapsed pattern through the scheduler and return the total steps. */
static long total_steps(const U32 *elapsed, int nFrames, U32 dt, S32 maxSteps) {
    U32 acc = 0;
    long steps = 0;
    for (int i = 0; i < nFrames; i++) {
        S32 n = Timer_FixedStepCount(&acc, elapsed[i], dt, maxSteps);
        steps += n;
        /* Post-condition: the carry never holds a whole step. */
        ASSERT_TRUE(acc < dt);
    }
    return steps;
}

/* Same total wall-time, different frame pacing, must yield the same number of 16 ms
 * simulation steps. 16000 ms is a whole number of 16 ms steps, so with no clamp the
 * count is exactly 1000 regardless of pacing. */
static void test_step_count_is_pacing_invariant(void) {
    const U32 DT = 16;
    const S32 NOCLAMP = -1;
    const long IDEAL = 16000 / 16; /* 1000 */

    U32 steady[1000];
    for (int i = 0; i < 1000; i++)
        steady[i] = 16; /* ~60 fps */

    static U32 fine[16000];
    for (int i = 0; i < 16000; i++)
        fine[i] = 1; /* ~1000 fps */

    U32 coarse[320];
    for (int i = 0; i < 320; i++)
        coarse[i] = 50; /* ~20 fps */

    /* Irregular but summing to 16000: alternate 8 and 24 ms frames. */
    U32 jittery[1000];
    for (int i = 0; i < 1000; i++)
        jittery[i] = (i & 1) ? 24 : 8;

    long s_steady = total_steps(steady, 1000, DT, NOCLAMP);
    long s_fine = total_steps(fine, 16000, DT, NOCLAMP);
    long s_coarse = total_steps(coarse, 320, DT, NOCLAMP);
    long s_jittery = total_steps(jittery, 1000, DT, NOCLAMP);

    printf("  steps over 16000 ms: steady=%ld fine=%ld coarse=%ld jittery=%ld (ideal %ld)\n",
           s_steady, s_fine, s_coarse, s_jittery, IDEAL);

    ASSERT_EQ_INT(IDEAL, s_steady);
    ASSERT_EQ_INT(IDEAL, s_fine);
    ASSERT_EQ_INT(IDEAL, s_coarse);
    ASSERT_EQ_INT(IDEAL, s_jittery);
}

/* High frame rate (elapsed < dt) runs the sim only every few frames; low frame rate
 * runs it several times per frame. */
static void test_high_and_low_frame_rate(void) {
    U32 acc = 0;
    /* ~250 fps: 4 ms frames, 16 ms step -> a step every 4th frame. */
    int nonzero = 0;
    for (int i = 0; i < 40; i++)
        if (Timer_FixedStepCount(&acc, 4, 16, -1) > 0)
            nonzero++;
    ASSERT_EQ_INT(10, nonzero); /* 40 frames * 4 ms / 16 ms */

    /* ~15 fps: one 66 ms frame owes 4 steps (66/16), carrying 2 ms. */
    acc = 0;
    ASSERT_EQ_INT(4, Timer_FixedStepCount(&acc, 66, 16, -1));
    ASSERT_EQ_UINT(2, acc);
}

/* A long stall (or a modal that banked real time while the game clock was frozen) is
 * clamped so the sim does not spiral trying to catch up; the backlog is dropped and only
 * the sub-dt remainder is kept. */
static void test_clamp_drops_backlog(void) {
    U32 acc = 0;
    /* 100003 ms of backlog, 16 ms step, at most 5 steps per frame. */
    S32 n = Timer_FixedStepCount(&acc, 100003, 16, 5);
    ASSERT_EQ_INT(5, n);
    ASSERT_EQ_UINT(100003 % 16, acc); /* remainder kept, backlog dropped */
}

/* A zero step size is a no-op guard, not a divide-by-zero, and does not consume time. */
static void test_zero_dt_guard(void) {
    U32 acc = 123;
    ASSERT_EQ_INT(0, Timer_FixedStepCount(&acc, 50, 0, 5));
    ASSERT_EQ_UINT(123, acc);
}

/* Timer_PlanSimSteps is the SHIPPED per-frame planner (SOURCES/PERSO.CPP MainLoop routes
 * through it). It layers the rewind and large-jump re-anchors over the Timer_FixedStepCount
 * core above. `base` anchors the sub-step clocks (sub-step k runs at base + (k+1)*dt); `next`
 * is written back to LastSimRefHR and carries the sub-dt remainder as (now - next). Pin every
 * branch. */
static void test_plan_sim_steps_cases(void) {
    const S32 DT = 16, MAX = 8;
    U32 base, next;

    /* ~60 fps: exactly one step, grid advances by dt, the step runs at `now`. This is the
     * historical per-frame path -- byte-identical to pre-throttle. */
    ASSERT_EQ_INT(1, Timer_PlanSimSteps(116, 100, DT, MAX, &base, &next));
    ASSERT_EQ_UINT(100, base); /* sub-step 0 at base + dt == 116 == now */
    ASSERT_EQ_UINT(116, next);

    /* High frame rate: less than a step has elapsed -> 0 steps, lastSim unchanged, the
     * remainder carries in the (now - next) gap. */
    ASSERT_EQ_INT(0, Timer_PlanSimSteps(108, 100, DT, MAX, &base, &next));
    ASSERT_EQ_UINT(100, next);

    /* Low frame rate, exact: elapsed = 4*dt -> 4 sub-steps, no remainder. */
    ASSERT_EQ_INT(4, Timer_PlanSimSteps(164, 100, DT, MAX, &base, &next));
    ASSERT_EQ_UINT(100, base);
    ASSERT_EQ_UINT(164, next);

    /* Low frame rate with remainder: elapsed = 50 -> 3 steps (48 ms), 2 ms carries. */
    ASSERT_EQ_INT(3, Timer_PlanSimSteps(150, 100, DT, MAX, &base, &next));
    ASSERT_EQ_UINT(100, base);
    ASSERT_EQ_UINT(148, next); /* 100 + 3*16; the 2 ms remainder is (150 - 148) */

    /* Boundary: elapsed == maxSteps*dt (128) is still the NORMAL path, not the large-jump
     * clamp -> 8 steps, full catch-up. */
    ASSERT_EQ_INT(8, Timer_PlanSimSteps(228, 100, DT, MAX, &base, &next));
    ASSERT_EQ_UINT(100, base);
    ASSERT_EQ_UINT(228, next);

    /* Just past the boundary (129 > 128): large jump -> drop the backlog, run one step, and
     * re-anchor the grid to now - dt so the single step runs at `now`. */
    ASSERT_EQ_INT(1, Timer_PlanSimSteps(229, 100, DT, MAX, &base, &next));
    ASSERT_EQ_UINT(229 - 16, base);
    ASSERT_EQ_UINT(229, next);

    /* First frame (lastSim seeded 0, now = a loaded save's large clock) is the large-jump
     * path: one step at `now`, NOT thousands of catch-up steps. */
    ASSERT_EQ_INT(1, Timer_PlanSimSteps(5351037, 0, DT, MAX, &base, &next));
    ASSERT_EQ_UINT(5351037 - 16, base);
    ASSERT_EQ_UINT(5351037, next);

    /* Rewind (now < lastSim: a modal SaveTimer restore / cube change): 0 steps, re-anchor
     * lastSim to now, skip the frame. */
    ASSERT_EQ_INT(0, Timer_PlanSimSteps(90, 100, DT, MAX, &base, &next));
    ASSERT_EQ_UINT(90, next);

    /* Throttle off (dt <= 0): one step, lastSim untouched (per-frame path). */
    ASSERT_EQ_INT(1, Timer_PlanSimSteps(200, 100, 0, MAX, &base, &next));
    ASSERT_EQ_UINT(100, next);
}

/* Drive the planner like the main loop does -- TimerRefHR advances by `frameDelta` each
 * rendered frame -- over a fixed span of game-time. Two invariants below the catch-up floor:
 *   (1) total sub-steps == span/dt regardless of the frame pacing (frame-rate independence);
 *   (2) the sub-step clocks tile the span contiguously -- each is exactly dt after the last
 *       one that ran, across frame boundaries, with no gap or overlap. */
static long plan_total_steps(U32 frameDelta, U32 span, S32 dt, S32 maxSteps) {
    U32 lastSim = 0, prevSubClock = 0;
    long steps = 0;
    for (U32 now = frameDelta; now <= span; now += frameDelta) {
        U32 base, next;
        S32 n = Timer_PlanSimSteps(now, lastSim, dt, maxSteps, &base, &next);
        for (S32 k = 0; k < n; k++) {
            U32 subClock = base + (U32)(k + 1) * (U32)dt;
            ASSERT_EQ_UINT(prevSubClock + (U32)dt, subClock); /* contiguous, exactly dt apart */
            prevSubClock = subClock;
        }
        lastSim = next;
        steps += n;
    }
    return steps;
}

static void test_plan_sim_steps_frame_invariance(void) {
    const U32 SPAN = 1600; /* whole number of 16 ms steps and divisible by every frameDelta below */
    const S32 DT = 16, MAX = 8;
    const long IDEAL = 1600 / 16; /* 100 */

    /* ~1000 / ~120 / ~60 / ~15 fps -- all at or below the ~8 fps catch-up floor. */
    const U32 rates[] = {1, 8, 16, 64};
    for (int i = 0; i < 4; i++) {
        long steps = plan_total_steps(rates[i], SPAN, DT, MAX);
        printf("  frameDelta=%2u ms -> %ld sub-steps over %u ms (ideal %ld)\n",
               rates[i], steps, SPAN, IDEAL);
        ASSERT_EQ_INT(IDEAL, steps);
    }
}

/* --- Modal clock steppers under fixed-dt ---------------------------------------------
 * Timer_FixedDtPresent()/Pump() keep the virtual clock moving inside modal inner loops so a
 * wait on a TimerRefHR deadline cannot deadlock. They mutate the private FixedDtNow;
 * ManageTime() copies that into the observable TimerSystemHR. Absolute values carry over
 * between tests (Timer_EnableFixedDt seeds FixedDtNow from the current TimerSystemHR), so
 * assert deltas against a captured base rather than literal zeros. */

static void test_fixed_dt_present_first_free_then_steps(void) {
    Timer_EnableFixedDt(16);
    ManageTime();
    U32 base = TimerSystemHR;

    /* Before the first tick advance the present is inert: boot/load presents must not move
     * the clock. */
    Timer_FixedDtPresent();
    ManageTime();
    ASSERT_EQ_UINT(base, TimerSystemHR);

    Timer_FixedDtAdvance(); /* one tick: +16, arms ticking, arms the free present */
    ManageTime();
    ASSERT_EQ_UINT(base + 16, TimerSystemHR);

    /* First present after the advance is free -- it is the main-loop render the tick already
     * paid for. */
    Timer_FixedDtPresent();
    ManageTime();
    ASSERT_EQ_UINT(base + 16, TimerSystemHR);

    /* Every additional present in the same tick steps the clock (modal inner-loop frames). */
    Timer_FixedDtPresent();
    ManageTime();
    ASSERT_EQ_UINT(base + 32, TimerSystemHR);
    Timer_FixedDtPresent();
    ManageTime();
    ASSERT_EQ_UINT(base + 48, TimerSystemHR);

    /* A fresh tick re-arms the free present. */
    Timer_FixedDtAdvance();
    ManageTime();
    ASSERT_EQ_UINT(base + 64, TimerSystemHR);
    Timer_FixedDtPresent();
    ManageTime();
    ASSERT_EQ_UINT(base + 64, TimerSystemHR);
}

static void test_fixed_dt_pump_steps_every_call(void) {
    Timer_EnableFixedDt(16);
    ManageTime();
    U32 base = TimerSystemHR;

    /* Inert before the first tick advance, same as present. */
    Timer_FixedDtPump();
    ManageTime();
    ASSERT_EQ_UINT(base, TimerSystemHR);

    Timer_FixedDtAdvance(); /* +16, arms ticking (and the free-present flag, which Pump ignores) */
    ManageTime();
    ASSERT_EQ_UINT(base + 16, TimerSystemHR);

    /* No free first step: a non-presenting busy-wait owns every iteration's time, so each Pump
     * advances the clock (this is what stops the MUSIC/AMBIANCE fade loops spinning forever on
     * a TimerRefHR deadline under fixed-dt). */
    Timer_FixedDtPump();
    ManageTime();
    ASSERT_EQ_UINT(base + 32, TimerSystemHR);
    Timer_FixedDtPump();
    ManageTime();
    ASSERT_EQ_UINT(base + 48, TimerSystemHR);
}

int main(void) {
    RUN_TEST(test_step_count_is_pacing_invariant);
    RUN_TEST(test_high_and_low_frame_rate);
    RUN_TEST(test_clamp_drops_backlog);
    RUN_TEST(test_zero_dt_guard);
    RUN_TEST(test_plan_sim_steps_cases);
    RUN_TEST(test_plan_sim_steps_frame_invariance);
    RUN_TEST(test_fixed_dt_present_first_free_then_steps);
    RUN_TEST(test_fixed_dt_pump_steps_every_call);
    TEST_SUMMARY();
    return test_failures != 0;
}
