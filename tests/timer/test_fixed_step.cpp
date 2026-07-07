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

int main(void) {
    RUN_TEST(test_step_count_is_pacing_invariant);
    RUN_TEST(test_high_and_low_frame_rate);
    RUN_TEST(test_clamp_drops_backlog);
    RUN_TEST(test_zero_dt_guard);
    TEST_SUMMARY();
    return test_failures != 0;
}
