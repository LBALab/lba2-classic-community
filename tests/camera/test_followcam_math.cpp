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

/* THE INTERIOR FOLLOW, which is a step over tiles rather than over angles (FOLLOWCAM_MATH.H's
 * `FollowCamIsoStep`). Same family of failure as the rotation lerp above and a different rule: an
 * isometric view is the tile it is centred on, so "following" there is two integers moving.
 *
 * The one property it must have is the one the classic recentre does not: it never passes the hero.
 * The classic line (`want + (want - now) / 2`) overshoots by design, which a one-off recentre of an
 * off-screen hero can afford and a camera running on consecutive frames cannot -- it would step past
 * him and back, once per tile. */
static void test_iso_step_never_passes_the_target(void) {
    S32 now, want;
    S32 bad = 0;

    for (now = -24; now <= 24 && !bad; now++) {
        for (want = -24; want <= 24; want++) {
            S32 next = FollowCamIsoStep(now, want);

            if (want == now) {
                if (next != now)
                    bad = 1;
            } else if (want > now) {
                if (next <= now || next > want)
                    bad = 1;
            } else if (next >= now || next < want) {
                bad = 1;
            }
            if (bad)
                break;
        }
    }
    ASSERT_EQ_INT(0, bad);
}

/* And it arrives: a gap of any size closes in a handful of frames, because half of what is left is a
 * step and a single tile is never truncated to none. The truncation is the failure that matters -- a
 * camera one brick behind the hero for ever, with the view dirty every frame -- and it is the same
 * failure FollowCamRotStep's own floor exists for. */
static void test_iso_step_arrives_in_a_few_frames(void) {
    S32 gap;
    S32 worst = 0;
    S32 stuck = 0;

    for (gap = -64; gap <= 64; gap++) {
        S32 at = 0;
        S32 frames = 0;

        while (at != gap) {
            S32 next = FollowCamIsoStep(at, gap);
            if (next == at) {
                stuck = 1;
                break;
            }
            at = next;
            if (++frames > 64) {
                stuck = 1;
                break;
            }
        }
        if (stuck)
            break;
        if (frames > worst)
            worst = frames;
    }

    ASSERT_EQ_INT(0, stuck);
    /* 64 tiles is six frames of halving; anything slower than a dozen is a camera a player drives
     * away from rather than one that follows. */
    ASSERT_TRUE(worst <= 12);
}

/* The contrast, stated as a test rather than as a comment: the classic line the follow replaced
 * overshoots, so reusing it here would fail the test above. Pinned on the arithmetic itself --
 * `want + (want - now) / 2` -- because the point is that the two rules are different, not that one
 * of them is a bug in the place it is used. */
static void test_the_classic_recentre_formula_overshoots(void) {
    S32 now = 0, want = 5;

    ASSERT_EQ_INT(7, want + (want - now) / 2);
    ASSERT_TRUE((want + (want - now) / 2) > want);
    /* and the follow's answer stays behind him */
    ASSERT_EQ_INT(2, FollowCamIsoStep(now, want));
}

/* THE PAN AND THE PROJECTION ARE THE SAME ARITHMETIC, and this is the test that says so. The brick
 * lattice is blitted on whole-tile pixels (GRILLE.CPP's `Map2Screen`) while objects and sprites are
 * projected (LPROJISO.CPP), so a camera between two tiles has to move both by one amount or the hero
 * slides across a grid that stands still.
 *
 * `iso_px`/`iso_py` below are `LongProjectPointIso` written out (it subtracts the camera, folds it
 * into two linear terms and adds the projection centre), which is the reference the pan has to
 * match. Checked over a brick's worth of lag in every direction rather than at a few points: the
 * failures are a sign and a shift, and both show up immediately. */
static S32 iso_px(S32 wx, S32 wz, S32 camx, S32 camz) {
    return (3 * ((wx - camx) - (wz - camz))) >> 6;
}

static S32 iso_py(S32 wx, S32 wy, S32 wz, S32 camx, S32 camy, S32 camz) {
    return (6 * (((wx - camx) + (wz - camz))) - 15 * (wy - camy)) >> 8;
}

static void test_iso_pan_matches_the_projection_over_a_brick(void) {
    const S32 brick = 512;
    S32 dx, dy, dz, bad = 0;

    for (dx = -brick; dx <= brick; dx += 17) {
        for (dz = -brick; dz <= brick; dz += 17) {
            for (dy = -brick; dy <= brick; dy += 64) {
                S32 panx = FollowCamIsoPanX(dx, dz);
                S32 pany = FollowCamIsoPanY(dx, dy, dz);

                /* A point on the tile the camera is anchored to, projected with the eye on the tile
                 * and again with it moved by the lag: the difference is the pan, to within the unit
                 * the two integer divisions truncate by. */
                /* `dx`/`dy`/`dz` are the eye's LAG: it sits at the anchor plus them, so the reference
                 * camera is the anchor moved by +d. */
                S32 refx = iso_px(3 * brick, 2 * brick, 3 * brick + dx, 2 * brick + dz) - iso_px(3 * brick, 2 * brick, 3 * brick, 2 * brick);
                S32 refy = iso_py(3 * brick, brick, 2 * brick, 3 * brick + dx, brick + dy, 2 * brick + dz) - iso_py(3 * brick, brick, 2 * brick, 3 * brick, brick, 2 * brick);

                if (panx - refx > 1 || panx - refx < -1)
                    bad++;
                if (pany - refy > 1 || pany - refy < -1)
                    bad++;
            }
        }
    }

    ASSERT_EQ_INT(0, bad);
}

/* And the units it is in: a brick of lag is exactly a brick of picture. `Map2Screen` steps 24 pixels
 * per tile across and 12 per tile down (15 per brick up), so these are the numbers a player sees --
 * a pan of 1 would be a pixel here, not a tile there. */
static void test_iso_pan_is_measured_in_bricks(void) {
    ASSERT_EQ_INT(0, FollowCamIsoPanX(0, 0));
    ASSERT_EQ_INT(0, FollowCamIsoPanY(0, 0, 0));

    /* The eye one brick further along +x, +z, and one brick up: content moves the other way. +x is
     * right and DOWN on an iso screen, so the eye moving +x takes the picture up (the 15-per-brick
     * rise for y is the same sign for the same reason). */
    ASSERT_EQ_INT(-24, FollowCamIsoPanX(512, 0));
    ASSERT_EQ_INT(24, FollowCamIsoPanX(0, 512));
    ASSERT_EQ_INT(-12, FollowCamIsoPanY(0, 0, 512));
    ASSERT_EQ_INT(-12, FollowCamIsoPanY(512, 0, 0));
    ASSERT_EQ_INT(15, FollowCamIsoPanY(0, 256, 0));

    /* Half a brick is half of that, and never a whole one: the point of the exercise. */
    ASSERT_EQ_INT(-12, FollowCamIsoPanX(256, 0));
}

/* An interior glide's length follows its distance: 6 ms a pixel between the setting's floor and the
 * 1.5 s ceiling, and a cut stays a cut. The square root is exact on the pixels that matter. */
static void test_iso_glide_length_follows_the_distance(void) {
    /* Short: the floor (cam_shot_blend_ms). */
    ASSERT_EQ_INT(600, FollowCamIsoGlideMs(0, 0, 600, 6, 1500));
    ASSERT_EQ_INT(600, FollowCamIsoGlideMs(30, 40, 600, 6, 1500)); /* 50 px, 300 ms */
    /* Between: 6 ms a pixel, Euclidean and either sign. */
    ASSERT_EQ_INT(1200, FollowCamIsoGlideMs(120, -160, 600, 6, 1500)); /* 200 px */
    ASSERT_EQ_INT(1200, FollowCamIsoGlideMs(-160, 120, 600, 6, 1500));
    ASSERT_EQ_INT(1014, FollowCamIsoGlideMs(169, 0, 600, 6, 1500));
    ASSERT_EQ_INT(1008, FollowCamIsoGlideMs(120, 118, 600, 6, 1500)); /* sqrt 28324 = 168.3, rounded down */
    /* Long: the ceiling, unless the floor is longer still. */
    ASSERT_EQ_INT(1500, FollowCamIsoGlideMs(3000, 3000, 600, 6, 1500));
    ASSERT_EQ_INT(2000, FollowCamIsoGlideMs(3000, 3000, 2000, 6, 1500));
    /* The largest pan a cube allows does not overflow. */
    ASSERT_EQ_INT(1500, FollowCamIsoGlideMs(0x7fffffff, 0x7fffffff, 600, 6, 1500));
    /* 0 is the cut, whatever the distance. */
    ASSERT_EQ_INT(0, FollowCamIsoGlideMs(300, 0, 0, 6, 1500));
    ASSERT_EQ_INT(0, FollowCamIsoGlideMs(300, 0, -5, 6, 1500));
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

/* Giving a scripted shot's yaw back: from any offset to any other, at any laziness, the ease must
 * arrive on the offset it owes and stop there. The pan drift it sits beside stops short whenever
 * what is left is smaller than its divisor, which for a return would leave the camera a degree or
 * two off the angle it was lent at, permanently. */
static void test_easing_a_pan_back_lands_exactly_from_everywhere(void) {
    S32 from, to, div;
    S32 bad = 0;

    for (div = 1; div <= 64 && !bad; div += 3) {
        for (to = 0; to < TURN && !bad; to += 331) {
            for (from = 0; from < TURN; from += 29) {
                S32 pan = from;
                S32 frames = 0;

                while (pan != to) {
                    pan = FollowCamEasePan(pan, to, div, FOLLOW_CAM_RETURN_YAW_MIN_STEP);
                    if (pan < 0 || pan >= TURN || ++frames > TURN) {
                        bad = 1;
                        break;
                    }
                }
                if (bad)
                    break;
                /* And stays: a further frame must not walk it off again. */
                if (FollowCamEasePan(pan, to, div, FOLLOW_CAM_RETURN_YAW_MIN_STEP) != to) {
                    bad = 1;
                    break;
                }
            }
        }
    }
    ASSERT_EQ_INT(0, bad);
}

/* Round the short way, including across the wrap: 106 units to go, not 3990. */
static void test_easing_a_pan_back_takes_the_short_way(void) {
    S32 pan = 4000;
    S32 frames = 0;
    S32 travelled = 0;

    while (pan != 10 && frames < TURN) {
        S32 next = FollowCamEasePan(pan, 10, FOLLOW_CAM_DRIFT_DIVISOR_DEFAULT, FOLLOW_CAM_RETURN_YAW_MIN_STEP);
        S32 moved = FollowCamAngleDiff(next, pan);
        if (moved <= 0)
            break; /* backwards or stalled: the two checks below report it, once */
        travelled += moved;
        pan = next;
        frames++;
    }
    ASSERT_EQ_INT(10, pan);
    ASSERT_EQ_INT(106, travelled);
}

/* The pitch is the same ease on a value that does not wrap. Over the whole range a shot can
 * author, it must reach the pitch it was lent at and never pass it. */
static void test_easing_a_pitch_back_lands_exactly_and_never_overshoots(void) {
    S32 from, to, div;
    S32 bad = 0;

    for (div = 1; div <= 64 && !bad; div += 3) {
        for (to = FOLLOW_CAM_ALPHA_MIN; to <= FOLLOW_CAM_ALPHA_MAX && !bad; to += 50) {
            for (from = -300; from <= 1000; from += 13) {
                S32 value = from;
                S32 frames = 0;

                while (value != to) {
                    S32 next = FollowCamEaseValue(value, to, div, FOLLOW_CAM_RETURN_PITCH_MIN_STEP);
                    /* Still on the side it started from: passing the target would tilt the
                     * camera through its resting pitch and back. */
                    if ((from < to && next > to) || (from > to && next < to) || ++frames > 4096) {
                        bad = 1;
                        break;
                    }
                    value = next;
                }
                if (bad)
                    break;
            }
        }
    }
    ASSERT_EQ_INT(0, bad);
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

/* The recompose is defined to be an exact no-op at the height the game was composed for, and the
 * boom is where that matters most: a length that came back one unit short at 480 would move the
 * camera on the one resolution that must look as it shipped. Checked over a wide spread of lengths
 * and tunings rather than at the two the engine currently runs with. */
static void test_hd_shrink_is_an_exact_identity_at_no_excess(void) {
    S32 len, gain, bad = 0;

    for (len = 0; len <= 50000; len += 137)
        for (gain = 0; gain <= 100; gain += 25)
            if (FollowCamHDShrink(len, 0, gain, 50) != len)
                bad++;

    ASSERT_EQ_INT(0, bad);
}

/* A negative excess is not reachable through FollowCamHDExcessFor, which floors at 0, but the rule
 * has to hold on its own terms: the recompose only ever shortens, and arithmetic on a negative
 * would lengthen the boom. Pinned so a later change to how the excess is derived cannot quietly
 * invert the correction. */
static void test_hd_shrink_never_lengthens_on_a_negative_excess(void) {
    ASSERT_EQ_INT(13750, FollowCamHDShrink(13750, -1, 27, 50));
    ASSERT_EQ_INT(13750, FollowCamHDShrink(13750, -125, 27, 50));
}

/* The documented working point: at 1080 the excess is 125, which at the shipped distance gain is a
 * 33% pull, taking the resting boom to 9212. The lean uses the same rule with its own tuning. */
static void test_hd_shrink_matches_the_documented_working_point(void) {
    S32 excess = FollowCamHDExcessFor(TRUE, 1080, FOLLOW_CAM_HD_EXCESS_CAP_DEFAULT);

    ASSERT_EQ_INT(125, excess);
    ASSERT_EQ_INT(9212, FollowCamHDShrink(13750, excess,
                                          FOLLOW_CAM_HD_DIST_GAIN_DEFAULT,
                                          FOLLOW_CAM_HD_DIST_MAX_PULL));
    ASSERT_EQ_INT(7035, FollowCamHDShrink(10500, excess,
                                          FOLLOW_CAM_HD_DIST_GAIN_DEFAULT,
                                          FOLLOW_CAM_HD_DIST_MAX_PULL));
}

/* Scaling then dividing once is not the same as subtracting the reduction: at the working point
 * above the two disagree by a unit. Both call sites have to round the same way, or the frame a
 * camera zone owns is a unit off the frames either side of it. This pins which one it is. */
static void test_hd_shrink_truncates_once_rather_than_subtracting(void) {
    S32 len = 13750, pull = 33;

    ASSERT_EQ_INT(len * (100 - pull) / 100, FollowCamHDShrink(len, 125, 27, 50));
    /* the other order, spelled out, is a different number */
    ASSERT_EQ_INT(9213, len - len * pull / 100);
    ASSERT_EQ_INT(9212, FollowCamHDShrink(len, 125, 27, 50));
}

/* The ceiling bounds the reduction, not the excess, so past it a taller frame converges on one
 * length instead of shrinking without bound. It must also never grow a length. */
static void test_hd_shrink_is_bounded_and_never_grows(void) {
    S32 excess, bad = 0, grew = 0;

    ASSERT_EQ_INT(6875, FollowCamHDShrink(13750, 100000, 27, 50)); /* pinned at the 50% ceiling */

    for (excess = 0; excess <= 4000; excess += 7) {
        S32 got = FollowCamHDShrink(13750, excess, 27, FOLLOW_CAM_HD_DIST_MAX_PULL);
        if (got < 13750 * (100 - FOLLOW_CAM_HD_DIST_MAX_PULL) / 100)
            bad++;
        if (got > 13750)
            grew++;
    }

    ASSERT_EQ_INT(0, bad);
    ASSERT_EQ_INT(0, grew);
}

/* How long the timed ease takes to bring a turn within 5%, in milliseconds, at one frame time. */
static S32 ease_ms_to_5_percent(S32 turn, S32 dt, S32 tau) {
    S32 left = turn;
    S32 carry = 0;
    S32 ms = 0;

    while (left > turn * 5 / 100 && ms < 100000) {
        left -= FollowCamEaseStepMs(left, dt, tau, FOLLOW_CAM_ROT_MIN_RATE, &carry);
        ms += dt;
    }
    return ms;
}

/* What the timed ease is for: the same turn takes the same time whatever the frame rate. The
 * per-frame lerp takes the same number of frames instead, so twice as long at 30 frames a second
 * as at 60. Checked from 250 frames a second down to 10, the longest frame the engine paces by. */
static void test_timed_ease_takes_the_same_time_at_any_frame_rate(void) {
    static const S32 dts[] = {4, 7, 8, 16, 17, 33, 50, 100};
    static const S32 taus[] = {FOLLOW_CAM_EASE_MS_DEFAULT, 600};
    S32 turn, i, t;
    S32 bad = 0;

    for (t = 0; t < 2; t++) {
        for (turn = 1024; turn <= 2048; turn += 1024) {
            S32 ref = ease_ms_to_5_percent(turn, 16, taus[t]);

            /* Three time constants bring an exponential within 5%. */
            if (ref < taus[t] * 3 - taus[t] / 4 || ref > taus[t] * 3 + taus[t] / 4)
                bad++;
            for (i = 0; i < (S32)(sizeof(dts) / sizeof(dts[0])); i++) {
                S32 ms = ease_ms_to_5_percent(turn, dts[i], taus[t]);
                /* Within a frame of the reference, or a twentieth of it. */
                S32 slack = (dts[i] > ref / 20) ? dts[i] : ref / 20;
                if (ms < ref - slack || ms > ref + slack)
                    bad++;
            }
        }
    }
    ASSERT_EQ_INT(0, bad);
}

/* From any distance, at any frame time and time constant, no step is away from the target or
 * past it, and repeated stepping arrives and stays. A frame far longer than the time constant
 * included: that is where a plain dt / tau overshoots. A step may be nothing at all on a short
 * frame, since what it earned is carried, so arriving is asserted as time and not per step. */
static void test_timed_ease_lands_exactly_and_never_overshoots(void) {
    static const S32 taus[] = {1, 50, 300, 600, 5000};
    S32 t, dt, diff;
    S32 bad = 0;

    for (t = 0; t < (S32)(sizeof(taus) / sizeof(taus[0])) && !bad; t++) {
        for (dt = 1; dt <= 1000 && !bad; dt += (dt < 100) ? 3 : 150) {
            for (diff = -2048; diff <= 2048; diff += 7) {
                S32 left = diff;
                S32 carry = 0;
                S32 ms = 0;

                while (left != 0) {
                    S32 step = FollowCamEaseStepMs(left, dt, taus[t], FOLLOW_CAM_ROT_MIN_RATE, &carry);
                    if ((left > 0 && (step < 0 || step > left)) || (left < 0 && (step > 0 || step < left))) {
                        bad = 1;
                        break;
                    }
                    left -= step;
                    ms += dt;
                    /* The slowest it may go is the floor, so a half turn is in by then. */
                    if (ms > 2048 * 1000 / FOLLOW_CAM_ROT_MIN_RATE + 2000) {
                        bad = 1;
                        break;
                    }
                }
                if (bad)
                    break;
                if (FollowCamEaseStepMs(0, dt, taus[t], FOLLOW_CAM_ROT_MIN_RATE, &carry) != 0 || carry != 0)
                    bad = 1;
            }
        }
    }
    ASSERT_EQ_INT(0, bad);
}

/* No time gone by is no move, however far there is to go: the first update after the clock is
 * restarted must not spend a step. And the settings these come from can be set to anything from
 * the console. */
static void test_timed_ease_waits_without_time_and_survives_bad_settings(void) {
    S32 carry = 0;

    ASSERT_EQ_INT(0, FollowCamEaseStepMs(1024, 0, 300, FOLLOW_CAM_ROT_MIN_RATE, &carry));
    ASSERT_EQ_INT(0, FollowCamEaseStepMs(-1024, -16, 300, FOLLOW_CAM_ROT_MIN_RATE, &carry));
    ASSERT_EQ_INT(0, carry);
    ASSERT_EQ_INT(1024, FollowCamEaseStepMs(1024, 16, 0, FOLLOW_CAM_ROT_MIN_RATE, &carry));
    ASSERT_EQ_INT(-1024, FollowCamEaseStepMs(-1024, 16, -300, FOLLOW_CAM_ROT_MIN_RATE, &carry));
    /* A distance the look-at point can trail by, at the longest frame and time constant. */
    ASSERT_EQ_INT(1980, FollowCamEaseStepMs(100000, 100, 5000, FOLLOW_CAM_TARGET_MIN_RATE, &carry));
    ASSERT_EQ_INT(198, carry);
    /* What is carried is for the direction it was earned in, and is dropped on a turn back. */
    ASSERT_EQ_INT(-1980, FollowCamEaseStepMs(-100000, 100, 5000, FOLLOW_CAM_TARGET_MIN_RATE, &carry));
    ASSERT_EQ_INT(-198, carry);
}

/* A floor of nothing still arrives: one unit to go, a long time constant and a short frame earn
 * no thousandths at all, and without a floor the camera would sit a unit off for ever. */
static void test_timed_ease_arrives_with_no_floor_asked_for(void) {
    S32 left = 1;
    S32 carry = 0;
    S32 ms = 0;

    while (left != 0 && ms < 5000) {
        left -= FollowCamEaseStepMs(left, 1, 5000, 0, &carry);
        ms++;
    }
    ASSERT_EQ_INT(0, left);
}

/* A blend into or out of a scripted shot starts on the view it leaves and ends on the one it
 * goes to, exactly, and never goes outside the two on the way. Eased in and out: it starts and
 * stops slowly, and is half way at half time. */
static void test_blend_weight_starts_ends_and_rises(void) {
    S32 ms, prev = -1;
    S32 bad = 0;

    ASSERT_EQ_INT(0, FollowCamBlendWeight(0, 600));
    ASSERT_EQ_INT(500, FollowCamBlendWeight(300, 600));
    ASSERT_EQ_INT(1000, FollowCamBlendWeight(600, 600));
    ASSERT_EQ_INT(1000, FollowCamBlendWeight(601, 600));
    /* No length is a cut, and so is a time before the start. */
    ASSERT_EQ_INT(1000, FollowCamBlendWeight(0, 0));
    ASSERT_EQ_INT(0, FollowCamBlendWeight(-16, 600));
    /* Slow at both ends: the first and last 16 ms frames move less than a middle one. */
    ASSERT_TRUE(FollowCamBlendWeight(16, 600) < FollowCamBlendWeight(316, 600) - FollowCamBlendWeight(300, 600));
    ASSERT_TRUE(1000 - FollowCamBlendWeight(584, 600) < FollowCamBlendWeight(316, 600) - FollowCamBlendWeight(300, 600));

    for (ms = 0; ms <= FOLLOW_CAM_SHOT_BLEND_MS_MAX; ms++) {
        S32 w = FollowCamBlendWeight(ms, FOLLOW_CAM_SHOT_BLEND_MS_MAX);
        if (w < prev || w < 0 || w > 1000)
            bad = 1;
        prev = w;
    }
    ASSERT_EQ_INT(0, bad);
}

/* The blended values: the ends are the two views, an angle goes the short way round and stays
 * inside the turn, and a world position as far as a scene is wide does not overflow. */
static void test_blend_values_take_the_short_way_and_do_not_overflow(void) {
    ASSERT_EQ_INT(1000, FollowCamBlendValue(1000, 9000, 0));
    ASSERT_EQ_INT(9000, FollowCamBlendValue(1000, 9000, 1000));
    ASSERT_EQ_INT(5000, FollowCamBlendValue(1000, 9000, 500));
    ASSERT_EQ_INT(-5000, FollowCamBlendValue(-1000, -9000, 500));
    ASSERT_EQ_INT(65536, FollowCamBlendValue(0, 65536, 1000));

    ASSERT_EQ_INT(4000, FollowCamBlendAngle(4000, 100, 0));
    ASSERT_EQ_INT(100, FollowCamBlendAngle(4000, 100, 1000));
    ASSERT_EQ_INT(2, FollowCamBlendAngle(4000, 100, 500)); /* across 0, not back round through 2048 */
    ASSERT_EQ_INT(2, FollowCamBlendAngle(100, 4000, 500));
    ASSERT_EQ_INT(4049, FollowCamBlendAngle(100, 4000, 750));
    ASSERT_EQ_INT(3072, FollowCamBlendAngle(-1024, -1024, 500));
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
    RUN_TEST(test_easing_a_pan_back_lands_exactly_from_everywhere);
    RUN_TEST(test_easing_a_pan_back_takes_the_short_way);
    RUN_TEST(test_easing_a_pitch_back_lands_exactly_and_never_overshoots);
    RUN_TEST(test_timed_ease_takes_the_same_time_at_any_frame_rate);
    RUN_TEST(test_timed_ease_lands_exactly_and_never_overshoots);
    RUN_TEST(test_timed_ease_waits_without_time_and_survives_bad_settings);
    RUN_TEST(test_timed_ease_arrives_with_no_floor_asked_for);
    RUN_TEST(test_blend_weight_starts_ends_and_rises);
    RUN_TEST(test_blend_values_take_the_short_way_and_do_not_overflow);
    RUN_TEST(test_hd_recompose_is_a_no_op_at_the_authored_height);
    RUN_TEST(test_hd_recompose_disabled_is_always_zero);
    RUN_TEST(test_hd_recompose_matches_the_documented_scale);
    RUN_TEST(test_hd_recompose_cap_bounds_tall_heights);
    RUN_TEST(test_hd_shrink_is_an_exact_identity_at_no_excess);
    RUN_TEST(test_hd_shrink_never_lengthens_on_a_negative_excess);
    RUN_TEST(test_hd_shrink_matches_the_documented_working_point);
    RUN_TEST(test_hd_shrink_truncates_once_rather_than_subtracting);
    RUN_TEST(test_hd_shrink_is_bounded_and_never_grows);
    RUN_TEST(test_iso_step_never_passes_the_target);
    RUN_TEST(test_iso_step_arrives_in_a_few_frames);
    RUN_TEST(test_the_classic_recentre_formula_overshoots);
    RUN_TEST(test_iso_pan_matches_the_projection_over_a_brick);
    RUN_TEST(test_iso_pan_is_measured_in_bricks);
    RUN_TEST(test_iso_glide_length_follows_the_distance);
    TEST_SUMMARY();
    return test_failures != 0;
}
