/* Host test for the Auto camera's angle arithmetic (SOURCES/FOLLOWCAM_MATH.H).
 * Header-only: no engine sources to link, runs anywhere with no retail data.
 *
 * The camera's own fixtures live in tests/automation and drive the real engine, so they need
 * retail data and a display and do not run in CI. What is here is the part that can: the two
 * relationships every camera fix so far has turned on, checked over their whole domain rather
 * than at the a few points a scripted run happens to visit.
 */
#include "FOLLOWCAM_CFG.H" /* the constants the engine actually runs with */
#include "FOLLOWCAM_MATH.H"
#include "test_harness.h"

#define TURN 4096
/* Taken from the config header rather than repeated here: the interesting failures are what
 * happens when these two are retuned relative to each other, and a copy would keep testing the
 * pair the engine no longer uses. */
#define SNAP_THRESHOLD FOLLOW_CAM_ROT_THRESHOLD
#define MIN_STEP FOLLOW_CAM_ROT_MIN_STEP

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
    /* At the half turn the sign of the subtraction is kept rather than resolved to one
     * direction, so the two ways round are opposites. Both callers share this, which is what
     * matters: a target exactly opposite is approached the same way by each of them. */
    ASSERT_EQ_INT(2048, FollowCamAngleDiff(2048, 0));
    ASSERT_EQ_INT(-2048, FollowCamAngleDiff(0, 2048));
}

static void test_angle_diff_is_always_within_a_half_turn(void) {
    S32 a, b;
    S32 bad = 0;

    for (a = 0; a < TURN && !bad; a += 13) {
        for (b = 0; b < TURN; b += 7) {
            S32 d = FollowCamAngleDiff(a, b);
            /* Both ends inclusive: the half turn is reachable with either sign. */
            if (d < -2048 || d > 2048 || ((b + d) & 4095) != a)
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
/* Standing on the target is not a reason to move. Promoting a zero step to the minimum would
 * walk the camera off by that much and back, for ever: the perpetual dirty frame the minimum step
 * exists to prevent, caused by the minimum step. Reachable only if a caller lowers its snap
 * threshold to zero, which is the kind of guarantee this function is meant not to depend on. */
static void test_no_step_when_already_on_target(void) {
    S32 div;

    for (div = 1; div <= 64; div++)
        ASSERT_EQ_INT(0, FollowCamRotStep(0, div, MIN_STEP));
}

/* The same, run as the caller would: from anywhere, with no threshold to hide behind, stepping
 * must come to rest rather than oscillate around the target. */
static void test_stepping_settles_without_a_snap_threshold(void) {
    S32 diff, div;
    S32 bad = 0;

    for (div = 1; div <= 64 && !bad; div++) {
        for (diff = -600; diff <= 600; diff += 7) {
            S32 left = diff;
            S32 frames = 0;

            while (left != 0) {
                left -= FollowCamRotStep(left, div, MIN_STEP);
                if (++frames > 4096) {
                    bad = 1;
                    break;
                }
            }
        }
    }
    ASSERT_EQ_INT(0, bad);
}

static void test_a_zero_divisor_is_survivable(void) {
    ASSERT_EQ_INT(100, FollowCamRotStep(100, 0, MIN_STEP));
    ASSERT_EQ_INT(-100, FollowCamRotStep(-100, -5, MIN_STEP));
}

/* The HD recompose's whole promise: at the height the game was composed for it does nothing at
 * all. Every recompose term scales by this, so a non-zero result at 480 would tilt and dolly the
 * camera on the one resolution that must look exactly as it shipped. Checked at 480 and below,
 * because a render height under 480 must not push the correction negative either. */
static void test_hd_recompose_is_a_no_op_at_the_authored_height(void) {
    S32 y, bad = 0;

    for (y = 1; y <= 480; y++) {
        if (FollowCamHDExcessFor(TRUE, y, 0) != 0)
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

/* Off means off, whatever the height. */
static void test_hd_recompose_disabled_is_always_zero(void) {
    ASSERT_EQ_INT(0, FollowCamHDExcessFor(FALSE, 720, 0));
    ASSERT_EQ_INT(0, FollowCamHDExcessFor(FALSE, 1080, 0));
    ASSERT_EQ_INT(0, FollowCamHDExcessFor(FALSE, 2160, 70));
}

/* The documented values: 0 at 480, 125 at 1080. These are what the tuning defaults in
 * FOLLOWCAM_CFG.H were chosen against, so a change to the formula that keeps the no-op at 480 but
 * moves 1080 would silently retune every HD gain. */
static void test_hd_recompose_matches_the_documented_scale(void) {
    ASSERT_EQ_INT(0, FollowCamHDExcessFor(TRUE, 480, 0));
    ASSERT_EQ_INT(50, FollowCamHDExcessFor(TRUE, 720, 0));
    ASSERT_EQ_INT(125, FollowCamHDExcessFor(TRUE, 1080, 0));
}

/* The cap is what stops the linear correction over-steepening at tall heights: past it, taller
 * resolutions converge on one fixed correction. 0 means no cap, which must stay linear rather
 * than clamping everything to zero. */
static void test_hd_recompose_cap_bounds_tall_heights(void) {
    S32 y, bad = 0;

    ASSERT_EQ_INT(70, FollowCamHDExcessFor(TRUE, 1080, 70));
    ASSERT_EQ_INT(70, FollowCamHDExcessFor(TRUE, 2160, 70));
    /* Below the cap the value is untouched. */
    ASSERT_EQ_INT(50, FollowCamHDExcessFor(TRUE, 720, 70));
    /* Cap 0 is "no cap", not "always zero". */
    ASSERT_EQ_INT(125, FollowCamHDExcessFor(TRUE, 1080, 0));

    /* Monotonic and never above the cap, across the range a player can reach. */
    for (y = 480; y <= 2160; y++) {
        S32 e = FollowCamHDExcessFor(TRUE, y, 70);
        if (e < 0 || e > 70)
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

int main(void) {
    RUN_TEST(test_pan_round_trips_for_every_angle);
    RUN_TEST(test_angles_stay_within_the_turn);
    RUN_TEST(test_angle_diff_takes_the_short_way);
    RUN_TEST(test_angle_diff_is_always_within_a_half_turn);
    RUN_TEST(test_step_moves_toward_the_target_without_overshooting);
    RUN_TEST(test_stepping_always_reaches_the_target);
    RUN_TEST(test_no_step_when_already_on_target);
    RUN_TEST(test_stepping_settles_without_a_snap_threshold);
    RUN_TEST(test_a_zero_divisor_is_survivable);
    RUN_TEST(test_hd_recompose_is_a_no_op_at_the_authored_height);
    RUN_TEST(test_hd_recompose_disabled_is_always_zero);
    RUN_TEST(test_hd_recompose_matches_the_documented_scale);
    RUN_TEST(test_hd_recompose_cap_bounds_tall_heights);
    TEST_SUMMARY();
    return test_failures != 0;
}
