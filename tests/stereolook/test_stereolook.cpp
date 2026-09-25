/* Host test: Quest-stereo head-look camera math (SOURCES/STEREOLOOK.CPP).
 *
 * The tracked pose layering lives on the headset (OpenXR eye views in), so
 * the pinable part is the convention set: the tracked head translation must
 * land on the same world axes the engine's own rotation (LongRotate) and
 * the BetaCam angle semantics use, or a lean goes sideways and a world-
 * anchored view drifts.  The exact-value cases below pin the metre scale,
 * the per-axis clamp and the view-relative lean direction; the sweep at the
 * end recomposes every offset through the real LongRotate and requires
 * agreement, so the helper and the engine rotation cannot drift apart.
 *
 * CPP-only (no ASM, no Docker, no game assets). */
#include "test_harness.h"

#include "C_EXTERN.H"

#include <3D/CAMERA.H>
#include <3D/ROT2D.H>

#include <math.h>

static const float PI = 3.14159265358979f;
/* Engine angles are 4096 per full turn (STEREOLOOK.CPP's own factor). */
static const float RAD_TO_ENGINE = 2048.0f / PI;

/* World units per metre and the clamp, mirrored from STEREOLOOK.CPP's
   documented constants — the tests below assert against them. */
static const float UNITS_PER_METRE = 320.0f;
static const float MAX_TRACKED_M = 0.5f;

/* LongRotate's output globals (defined in LIB386/3D/CAMERA.CPP, which pulls
   the whole camera stack when linked — the two S32s are all this test needs). */
S32 X0 = 0;
S32 Z0 = 0;

/* Shortest signed distance between two engine angles (the helper truncates
   its radian products, so composed cases allow one step of slack). */
static S32 angle_dist(S32 a, S32 b) {
    S32 d = (a - b) & 4095;
    if (d > 2048)
        d -= 4096;
    return d < 0 ? -d : d;
}

static void test_view_beta_composes_and_wraps(void) {
    /* 90 degrees of head turn = 1024 engine angles. */
    ASSERT_TRUE(angle_dist(StereoLookViewBeta(2048, PI / 2.0f), 3072) <= 1);
    ASSERT_TRUE(angle_dist(StereoLookViewBeta(2048, -PI / 2.0f), 1024) <= 1);
    /* Wraps to a turn, including through the seam. */
    ASSERT_TRUE(angle_dist(StereoLookViewBeta(4000, PI / 2.0f), 928) <= 1);
    ASSERT_TRUE(angle_dist(StereoLookViewBeta(100, -PI), 2148) <= 1);
    /* Zero offset keeps the base exactly. */
    ASSERT_EQ_INT(1234, StereoLookViewBeta(1234, 0.0f));
}

static void test_view_alpha_sign(void) {
    /* Raising the pitch looks further up: AlphaCam must decrease. */
    ASSERT_EQ_INT(150 - (S32)(0.3f * RAD_TO_ENGINE),
                  StereoLookViewAlpha(150, 0.3f));
    ASSERT_EQ_INT(150 - (S32)(-0.3f * RAD_TO_ENGINE),
                  StereoLookViewAlpha(150, -0.3f));
    ASSERT_EQ_INT(150, StereoLookViewAlpha(150, 0.0f));
}

static void test_head_offset_zero(void) {
    S32 x = 99, y = 99, z = 99;
    StereoLookHeadOffsetToWorld(0.0f, 0.0f, 0.0f, 2048, &x, &y, &z);
    ASSERT_EQ_INT(0, x);
    ASSERT_EQ_INT(0, y);
    ASSERT_EQ_INT(0, z);
}

/* At BetaCam 2048 the tracked reference frame aligns with the world axes:
   forward lean is +Z, a rightward lean is -X (right-handed, Y up), up lean
   is +Y. */
static void test_head_offset_axes_at_reference(void) {
    S32 x, y, z;
    StereoLookHeadOffsetToWorld(0.0f, 0.0f, -0.25f, 2048, &x, &y, &z);
    ASSERT_EQ_INT(0, x);
    ASSERT_EQ_INT(0, y);
    ASSERT_EQ_INT((S32)(0.25f * UNITS_PER_METRE), z);

    StereoLookHeadOffsetToWorld(0.1f, 0.0f, 0.0f, 2048, &x, &y, &z);
    ASSERT_EQ_INT((S32)(-0.1f * UNITS_PER_METRE), x);
    ASSERT_EQ_INT(0, y);
    ASSERT_EQ_INT(0, z);

    StereoLookHeadOffsetToWorld(0.0f, 0.1f, 0.0f, 2048, &x, &y, &z);
    ASSERT_EQ_INT(0, x);
    ASSERT_EQ_INT((S32)(0.1f * UNITS_PER_METRE), y);
    ASSERT_EQ_INT(0, z);
}

/* A quarter turn of view yaw (BetaCam 1024) rotates the lean with it. */
static void test_head_offset_follows_view_yaw(void) {
    S32 x, y, z;
    StereoLookHeadOffsetToWorld(0.0f, 0.0f, -0.25f, 1024, &x, &y, &z);
    ASSERT_EQ_INT((S32)(0.25f * UNITS_PER_METRE), x);
    ASSERT_EQ_INT(0, y);
    ASSERT_NEAR_F(0.0f, (float)z, 1.0f);

    StereoLookHeadOffsetToWorld(0.1f, 0.0f, 0.0f, 1024, &x, &y, &z);
    ASSERT_NEAR_F(0.0f, (float)x, 1.0f);
    ASSERT_EQ_INT(0, y);
    ASSERT_EQ_INT((S32)(0.1f * UNITS_PER_METRE), z);
}

static void test_head_offset_clamps_per_axis(void) {
    S32 x, y, z;
    StereoLookHeadOffsetToWorld(5.0f, 5.0f, -5.0f, 2048, &x, &y, &z);
    ASSERT_EQ_INT((S32)(-MAX_TRACKED_M * UNITS_PER_METRE), x);
    ASSERT_EQ_INT((S32)(MAX_TRACKED_M * UNITS_PER_METRE), y);
    ASSERT_EQ_INT((S32)(MAX_TRACKED_M * UNITS_PER_METRE), z);
}

/* The convention pin: the helper must agree with composing the same lean
   out of the engine's own LongRotate — forward along 2048-B, right along
   1024-B — for every view yaw, so the lean can never drift off the axes
   the renderer actually uses. */
static void test_head_offset_matches_long_rotate(void) {
    struct {
        S32 beta;
    } cases[] = {{0}, {37}, {512}, {1024}, {2048}, {3001}, {4095}, {4096}};
    const S32 fwd = 100;    /* metres: -0.3125 * 320 */
    const S32 right = 32;   /* metres: 0.1 * 320 */
    S32 i;
    for (i = 0; i < (S32)(sizeof(cases) / sizeof(cases[0])); i++) {
        S32 x, y, z;
        S32 fwdX, fwdZ, rightX, rightZ;
        StereoLookHeadOffsetToWorld(0.1f, 0.0f, -0.3125f, cases[i].beta, &x,
                                    &y, &z);
        LongRotate(0, fwd, (S32)(2048 - cases[i].beta) & 4095);
        fwdX = X0;
        fwdZ = Z0;
        LongRotate(0, right, (S32)(1024 - cases[i].beta) & 4095);
        rightX = X0;
        rightZ = Z0;
        ASSERT_NEAR_F((float)(fwdX + rightX), (float)x, 1.0f);
        ASSERT_NEAR_F((float)(fwdZ + rightZ), (float)z, 1.0f);
        ASSERT_EQ_INT(0, y);
    }
}

int main(void) {
    RUN_TEST(test_view_beta_composes_and_wraps);
    RUN_TEST(test_view_alpha_sign);
    RUN_TEST(test_head_offset_zero);
    RUN_TEST(test_head_offset_axes_at_reference);
    RUN_TEST(test_head_offset_follows_view_yaw);
    RUN_TEST(test_head_offset_clamps_per_axis);
    RUN_TEST(test_head_offset_matches_long_rotate);
    TEST_SUMMARY();
    return test_failures != 0;
}
