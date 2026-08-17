/* Host test for the Auto camera's angle arithmetic (SOURCES/FOLLOWCAM_MATH.H).
 * Header-only: no engine sources to link, runs anywhere with no retail data.
 *
 * The camera's own fixtures live in tests/automation and drive the real engine, so they need
 * retail data and a display and do not run in CI. What is here is the part that can: the two
 * relationships every camera fix so far has turned on, checked over their whole domain rather
 * than at the a few points a scripted run happens to visit.
 */
#include "FOLLOWCAM_MATH.H"
#include "test_harness.h"

#define TURN 4096
#define SNAP_THRESHOLD 10 /* FOLLOW_CAM_ROT_THRESHOLD: callers snap within this */
#define MIN_STEP 3        /* FOLLOW_CAM_ROT_MIN_STEP */

/* The relationship the whole camera rests on: aiming at the angle the camera already holds must
 * leave it there. Three shipped bugs were a writer of BetaCam breaking this, so it is checked for
 * every hero facing against every camera angle rather than sampled. */
static void test_pan_round_trips_for_every_angle(void) {
    S32 hero, beta;
    S32 bad = 0;

    for (hero = 0; hero < TURN && !bad; hero++) {
        for (beta = 0; beta < TURN; beta++) {
            S32 pan = FollowCamPanForAngle(beta, hero);
            if (FollowCamTargetBetaFor(pan, hero) != beta) {
                bad = 1;
                break;
            }
        }
    }
    ASSERT_EQ_INT(0, bad);
}

/* Both directions stay inside the circle, whatever they are handed. A negative or out-of-range
 * angle reaching a renderer is a wrapped view rather than a wrong one, which is harder to spot. */
static void test_angles_stay_within_the_turn(void) {
    S32 i;
    S32 bad = 0;

    for (i = -TURN * 2; i <= TURN * 2 && !bad; i += 7) {
        S32 pan = FollowCamPanForAngle(i, i / 3);
        S32 target = FollowCamTargetBetaFor(i, i / 3);
        if (pan < 0 || pan >= TURN || target < 0 || target >= TURN)
            bad = 1;
    }
    ASSERT_EQ_INT(0, bad);
}

/* The shortest way round, and never the long one. */
static void test_angle_diff_takes_the_short_way(void) {
    ASSERT_EQ_INT(0, FollowCamAngleDiff(100, 100));
    ASSERT_EQ_INT(10, FollowCamAngleDiff(110, 100));
    ASSERT_EQ_INT(-10, FollowCamAngleDiff(100, 110));
    /* Across the wrap: 10 units apart, not 4086. */
    ASSERT_EQ_INT(20, FollowCamAngleDiff(10, 4086));
    ASSERT_EQ_INT(-20, FollowCamAngleDiff(4086, 10));
    /* The half turn is the boundary and resolves one way, not both. */
    ASSERT_EQ_INT(2048, FollowCamAngleDiff(2048, 0));
}

static void test_angle_diff_is_always_within_a_half_turn(void) {
    S32 a, b;
    S32 bad = 0;

    for (a = 0; a < TURN && !bad; a += 13) {
        for (b = 0; b < TURN; b += 7) {
            S32 d = FollowCamAngleDiff(a, b);
            if (d <= -2048 || d > 2048 || ((b + d) & 4095) != a)
                bad = 1;
        }
    }
    ASSERT_EQ_INT(0, bad);
}

/* Every step is toward the target and never past it. Overshoot would leave the camera
 * approaching from the far side, which reads as a wobble rather than a settle. */
static void test_step_moves_toward_the_target_without_overshooting(void) {
    S32 diff, div;
    S32 bad = 0;

    for (div = 1; div <= 64 && !bad; div++) {
        for (diff = -2048; diff <= 2048; diff++) {
            S32 step = FollowCamRotStep(diff, div, MIN_STEP);
            if (diff == 0)
                continue;
            if ((diff > 0 && (step <= 0 || step > diff)) || (diff < 0 && (step >= 0 || step < diff))) {
                bad = 1;
                break;
            }
        }
    }
    ASSERT_EQ_INT(0, bad);
}

/* The property the minimum step exists for: from anywhere past the snap threshold, repeated
 * stepping closes the gap. The failure it guards against is not a wrong angle but a camera that
 * never arrives, holding the dirty check open and re-rendering the exterior every idle frame, so
 * it is asserted as termination rather than as a value.
 */
static void test_stepping_always_reaches_the_target(void) {
    S32 diff, div;
    S32 worst = 0;
    S32 stuck = 0;

    for (div = 1; div <= 64 && !stuck; div++) {
        for (diff = -2048; diff <= 2048; diff++) {
            S32 left = diff;
            S32 frames = 0;

            if (left > -SNAP_THRESHOLD && left < SNAP_THRESHOLD)
                continue; /* the caller snaps here rather than stepping */

            while (left > SNAP_THRESHOLD || left < -SNAP_THRESHOLD) {
                S32 step = FollowCamRotStep(left, div, MIN_STEP);
                if (step == 0) {
                    stuck = 1; /* would never arrive */
                    break;
                }
                left -= step;
                if (++frames > 4096) {
                    stuck = 1; /* arriving, but not in any useful time */
                    break;
                }
            }
            if (stuck)
                break;
            if (frames > worst)
                worst = frames;
        }
    }

    ASSERT_EQ_INT(0, stuck);
    /* A half turn at the laziest divisor tested is the slow case; well inside a second at 60fps
     * once the caller's snap threshold finishes it off. */
    ASSERT_TRUE(worst < 700);
}

/* A divisor of zero would be a division fault rather than a slow camera, and the console can set
 * the cvars these come from to anything. */
static void test_a_zero_divisor_is_survivable(void) {
    ASSERT_EQ_INT(100, FollowCamRotStep(100, 0, MIN_STEP));
    ASSERT_EQ_INT(-100, FollowCamRotStep(-100, -5, MIN_STEP));
}

int main(void) {
    RUN_TEST(test_pan_round_trips_for_every_angle);
    RUN_TEST(test_angles_stay_within_the_turn);
    RUN_TEST(test_angle_diff_takes_the_short_way);
    RUN_TEST(test_angle_diff_is_always_within_a_half_turn);
    RUN_TEST(test_step_moves_toward_the_target_without_overshooting);
    RUN_TEST(test_stepping_always_reaches_the_target);
    RUN_TEST(test_a_zero_divisor_is_survivable);
    TEST_SUMMARY();
}
