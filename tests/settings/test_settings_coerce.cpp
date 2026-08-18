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
 * as unset, the Auto camera's toggles take any non-zero as on, AllCameras has no range at all.
 *
 * value is set to NULL: Settings_Coerce answers from the row alone and never dereferences it,
 * which is exactly what makes it reachable from here with no engine to link. */
static T_SETTING row(T_SETTING_TYPE type, S32 def, S32 min, S32 max) {
    T_SETTING s;
    s.key = "K";
    s.legacy = NULL;
    s.cvar = NULL;
    s.value = NULL;
    s.type = type;
    s.def = def;
    s.min = min;
    s.max = max;
    s.stored = NULL;
    s.forced = NULL;
    s.on_change = NULL;
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

/* A run the flag was not given persists whatever the setting holds. This is every setting on a
 * normal launch, so getting it wrong would stop the options menu saving anything. */
static void test_an_unforced_run_persists_the_live_value(void) {
    ASSERT_EQ_INT(33, Settings_ValueToPersist(33, 16, -1));
    ASSERT_EQ_INT(0, Settings_ValueToPersist(0, 16, -1));
    ASSERT_EQ_INT(16, Settings_ValueToPersist(16, 16, -1));
}

/* A flag that says "this run only" must not leave its value behind. `--fixed-timestep 100` on a
 * cfg holding 16 has to persist 16, or one harness run silently throttles every later launch. */
static void test_a_forced_value_is_not_left_behind(void) {
    ASSERT_EQ_INT(16, Settings_ValueToPersist(100, 16, 100));
    ASSERT_EQ_INT(1, Settings_ValueToPersist(0, 1, 0)); /* --vsync off over a cfg holding on */
    ASSERT_EQ_INT(0, Settings_ValueToPersist(2, 0, 2)); /* LBA2_TEXFILTER=2 over a cfg holding off */
}

/* Changing the setting during an overridden run does persist: the live value has moved off what
 * the flag forced, so it is the player's choice rather than the flag's. */
static void test_changing_it_during_an_overridden_run_persists(void) {
    ASSERT_EQ_INT(50, Settings_ValueToPersist(50, 16, 100));
    ASSERT_EQ_INT(2, Settings_ValueToPersist(2, 0, 1));
}

/* The known limit, pinned so it stays a decision rather than a surprise: the comparison is on the
 * value, so setting a setting back to exactly what the flag forced is indistinguishable from
 * leaving it alone, and does not persist. Settings have no on-change hook to tell the two apart. */
static void test_setting_it_to_the_forced_value_cannot_be_told_apart(void) {
    ASSERT_EQ_INT(16, Settings_ValueToPersist(100, 16, 100));
}

/* A forced value of 0 is still a forced value: -1 is the only "not forced". Treating 0 as absent
 * would make `--vsync off` persist every time. */
static void test_zero_is_a_real_forced_value(void) {
    ASSERT_EQ_INT(1, Settings_ValueToPersist(0, 1, 0));
    ASSERT_EQ_INT(0, Settings_ValueToPersist(0, 1, -1));
}

/* A row whose default sits outside its own range is a declaration mistake, and falling back to it
 * would hand out exactly the value this rule refuses. The fallback is bounded too, so a bad row
 * cannot produce an out-of-range live setting. */
static void test_a_default_outside_the_range_is_contained(void) {
    const T_SETTING high = row(SETTING_OR_DEFAULT, 999, 0, 2);
    const T_SETTING low = row(SETTING_OR_DEFAULT, -5, 0, 2);

    ASSERT_EQ_INT(2, Settings_Coerce(&high, 99));
    ASSERT_EQ_INT(0, Settings_Coerce(&low, 99));
    /* An in-range value is still returned untouched, bad default or not. */
    ASSERT_EQ_INT(1, Settings_Coerce(&high, 1));
}

/* --- Settings_Apply: storing a value and telling its owner ---------------- */

/* A hook that records what it was told, so a test can ask whether it ran, how
   often, and with which value. */
static S32 s_applied_calls = 0;
static S32 s_applied_value = -1;

static void record_apply(S32 value) {
    s_applied_calls++;
    s_applied_value = value;
}

static void reset_apply(void) {
    s_applied_calls = 0;
    s_applied_value = -1;
}

/* The hook is told what the setting holds, not what arrived. A hook handed the
   raw value would apply something the setting does not hold, which for a driver
   means the two disagree in the direction this exists to prevent. */
static void test_the_hook_is_told_the_stored_value(void) {
    S32 live = 0;
    T_SETTING s = row(SETTING_CLAMP, 3, 0, 10);
    s.value = &live;
    s.on_change = record_apply;

    reset_apply();
    Settings_Apply(&s, 99);
    ASSERT_EQ_INT(10, live);
    ASSERT_EQ_INT(1, s_applied_calls);
    ASSERT_EQ_INT(10, s_applied_value);
}

/* Fires once per write, including when the value did not move. A setting reapplied
   to what it already held still has to reach whatever caches it: the console's
   reverse-stereo verb run twice must leave the driver swapped, not toggled. */
static void test_the_hook_fires_on_every_write(void) {
    S32 live = 0;
    T_SETTING s = row(SETTING_RAW, 0, SETTING_MIN_NONE, SETTING_MAX_NONE);
    s.value = &live;
    s.on_change = record_apply;

    reset_apply();
    Settings_Apply(&s, 1);
    Settings_Apply(&s, 1);
    ASSERT_EQ_INT(2, s_applied_calls);
    ASSERT_EQ_INT(1, live);
}

/* Most settings have no hook, and storing must not care. */
static void test_a_setting_without_a_hook_just_stores(void) {
    S32 live = 0;
    T_SETTING s = row(SETTING_CLAMP, 3, 0, 10);
    s.value = &live;
    s.on_change = NULL;

    reset_apply();
    Settings_Apply(&s, 7);
    ASSERT_EQ_INT(7, live);
    ASSERT_EQ_INT(0, s_applied_calls);
}

int main(void) {
    RUN_TEST(test_every_rule_passes_an_in_range_value_through);
    RUN_TEST(test_clamp_moves_to_the_nearer_bound);
    RUN_TEST(test_or_default_falls_back_rather_than_clamping);
    RUN_TEST(test_truthy_maps_every_non_zero_to_one);
    RUN_TEST(test_raw_keeps_whatever_the_cfg_held);
    RUN_TEST(test_the_settings_that_a_collapsed_rule_moved);
    RUN_TEST(test_the_unbounded_sentinels_clamp_to_nothing);
    RUN_TEST(test_an_unforced_run_persists_the_live_value);
    RUN_TEST(test_a_forced_value_is_not_left_behind);
    RUN_TEST(test_changing_it_during_an_overridden_run_persists);
    RUN_TEST(test_setting_it_to_the_forced_value_cannot_be_told_apart);
    RUN_TEST(test_zero_is_a_real_forced_value);
    RUN_TEST(test_a_default_outside_the_range_is_contained);
    RUN_TEST(test_the_hook_is_told_the_stored_value);
    RUN_TEST(test_the_hook_fires_on_every_write);
    RUN_TEST(test_a_setting_without_a_hook_just_stores);
    TEST_SUMMARY();
    return test_failures != 0;
}
