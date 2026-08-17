/* Host test for the settings coercion rules (SOURCES/SETTINGS.H).
 * Header-only: no engine sources to link, runs anywhere with no retail data.
 *
 * This is the half of the settings layer that can be tested without booting: what a cfg value
 * becomes once a setting's declared range is applied. The rest of the layer talks to the cfg
 * reader and the console, and is covered by the round-trip comparisons in tests/automation.
 *
 * Why these cases and not a sample: the engine applies four different rules to an out-of-range
 * value, and giving every setting one of two behaviours instead moved six of them to a different
 * boot value. A cfg the engine wrote itself holds only in-range values, so no read-write-compare
 * check can catch that class. These are the four rules, exercised on the values that separate them.
 */
#include "SETTINGS.H"
#include "test_harness.h"

/* A row per rule, matching real declarations: DetailLevel clamps, FullScreen treats out of range
 * as unset, the Auto camera's toggles take any non-zero as on, AllCameras has no range at all. */
static S32 storage;

static T_SETTING row(T_SETTING_TYPE type, S32 def, S32 min, S32 max) {
    T_SETTING s;
    s.key = "K";
    s.legacy = NULL;
    s.cvar = NULL;
    s.value = &storage;
    s.type = type;
    s.def = def;
    s.min = min;
    s.max = max;
    s.stored = NULL;
    s.forced = NULL;
    s.help = NULL;
    return s;
}

/* In range, every rule leaves the value alone. The rules may only differ outside it. */
static void test_every_rule_passes_an_in_range_value_through(void) {
    const T_SETTING clamp = row(SETTING_CLAMP, 3, 0, 10);
    const T_SETTING ordef = row(SETTING_OR_DEFAULT, 3, 0, 10);
    const T_SETTING raw = row(SETTING_RAW, 3, SETTING_MIN_NONE, SETTING_MAX_NONE);
    S32 v;

    for (v = 0; v <= 10; v++) {
        ASSERT_EQ_INT(v, Settings_Coerce(&clamp, v));
        ASSERT_EQ_INT(v, Settings_Coerce(&ordef, v));
        ASSERT_EQ_INT(v, Settings_Coerce(&raw, v));
    }
}

/* Clamp goes to the nearer bound. DetailLevel is the live example. */
static void test_clamp_moves_to_the_nearer_bound(void) {
    const T_SETTING s = row(SETTING_CLAMP, 3, 0, 10);

    ASSERT_EQ_INT(10, Settings_Coerce(&s, 99));
    ASSERT_EQ_INT(0, Settings_Coerce(&s, -1));
    ASSERT_EQ_INT(0, Settings_Coerce(&s, -99999));
}

/* Out of range means the cfg did not say anything usable, so the default stands. This is the rule
 * that FullScreen, DisplayFullScreen, DitherShading, FollowCamera and TextureFilter use, and the
 * one that clamping silently replaced: TextureFilter 99 must come back off, not bilinear. */
static void test_or_default_falls_back_rather_than_clamping(void) {
    const T_SETTING boolish = row(SETTING_OR_DEFAULT, 0, 0, 1);
    const T_SETTING texfilter = row(SETTING_OR_DEFAULT, 0, 0, 2);

    ASSERT_EQ_INT(0, Settings_Coerce(&boolish, 5));
    ASSERT_EQ_INT(0, Settings_Coerce(&boolish, -1));
    ASSERT_EQ_INT(0, Settings_Coerce(&texfilter, 99));
    ASSERT_EQ_INT(2, Settings_Coerce(&texfilter, 2)); /* the top of the range is still in it */

    /* A default of TRUE falls back to TRUE, not to the low bound. */
    const T_SETTING fullscreen = row(SETTING_OR_DEFAULT, 1, 0, 1);
    ASSERT_EQ_INT(1, Settings_Coerce(&fullscreen, 5));
}

/* Any non-zero is on. The Auto camera's toggles read this way. */
static void test_truthy_maps_every_non_zero_to_one(void) {
    const T_SETTING s = row(SETTING_TRUTHY, 0, 0, 1);
    S32 v, bad = 0;

    ASSERT_EQ_INT(0, Settings_Coerce(&s, 0));
    for (v = -50; v <= 50; v++) {
        if (v == 0)
            continue;
        if (Settings_Coerce(&s, v) != 1)
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

/* No range at all: whatever the cfg held stands, including values a reader might call absurd.
 * AllCameras and ReverseStereo are declared this way because that is what the engine does with
 * them, and quietly normalising them would change what an existing cfg boots into. */
static void test_raw_keeps_whatever_the_cfg_held(void) {
    const T_SETTING s = row(SETTING_RAW, 1, SETTING_MIN_NONE, SETTING_MAX_NONE);

    ASSERT_EQ_INT(5, Settings_Coerce(&s, 5));
    ASSERT_EQ_INT(7, Settings_Coerce(&s, 7));
    ASSERT_EQ_INT(-3, Settings_Coerce(&s, -3));
    ASSERT_EQ_INT(0, Settings_Coerce(&s, 0));
}

/* The six settings whose boot value moved when the rules were collapsed to two, as a regression
 * case in their own right: each pairs the value a cfg might hold with what it must become. */
static void test_the_settings_that_a_collapsed_rule_moved(void) {
    const T_SETTING all_cameras = row(SETTING_RAW, 1, SETTING_MIN_NONE, SETTING_MAX_NONE);
    const T_SETTING reverse_stereo = row(SETTING_RAW, 0, SETTING_MIN_NONE, SETTING_MAX_NONE);
    const T_SETTING follow_camera = row(SETTING_OR_DEFAULT, 0, 0, 1);
    const T_SETTING display_fullscreen = row(SETTING_OR_DEFAULT, 0, 0, 1);
    const T_SETTING dither = row(SETTING_OR_DEFAULT, 0, 0, 1);
    const T_SETTING texture_filter = row(SETTING_OR_DEFAULT, 0, 0, 2);

    ASSERT_EQ_INT(5, Settings_Coerce(&all_cameras, 5));
    ASSERT_EQ_INT(7, Settings_Coerce(&reverse_stereo, 7));
    ASSERT_EQ_INT(0, Settings_Coerce(&follow_camera, 5));
    ASSERT_EQ_INT(0, Settings_Coerce(&display_fullscreen, 5));
    ASSERT_EQ_INT(0, Settings_Coerce(&dither, 5));
    ASSERT_EQ_INT(0, Settings_Coerce(&texture_filter, 99));
}

/* The unbounded sentinels have to clamp to nothing, or a setting declared unbounded would be
 * pinned to whatever the sentinel is. */
static void test_the_unbounded_sentinels_clamp_to_nothing(void) {
    const T_SETTING s = row(SETTING_CLAMP, 0, SETTING_MIN_NONE, SETTING_MAX_NONE);

    ASSERT_EQ_INT(1000000, Settings_Coerce(&s, 1000000));
    ASSERT_EQ_INT(-1000000, Settings_Coerce(&s, -1000000));
    ASSERT_EQ_INT(SETTING_MAX_NONE, Settings_Coerce(&s, SETTING_MAX_NONE));
    ASSERT_EQ_INT(SETTING_MIN_NONE, Settings_Coerce(&s, SETTING_MIN_NONE));
}

int main(void) {
    RUN_TEST(test_every_rule_passes_an_in_range_value_through);
    RUN_TEST(test_clamp_moves_to_the_nearer_bound);
    RUN_TEST(test_or_default_falls_back_rather_than_clamping);
    RUN_TEST(test_truthy_maps_every_non_zero_to_one);
    RUN_TEST(test_raw_keeps_whatever_the_cfg_held);
    RUN_TEST(test_the_settings_that_a_collapsed_rule_moved);
    RUN_TEST(test_the_unbounded_sentinels_clamp_to_nothing);
    TEST_SUMMARY();
    return test_failures != 0;
}
