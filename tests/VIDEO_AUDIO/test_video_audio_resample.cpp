/* Test: ResampleAndAddToMix and GetEffectiveTrackRate — video audio resampling.
 * CPP-only (no ASM); uses synthetic buffers. No game assets required. */
#include "test_harness.h"

#include <AIL/VIDEO_AUDIO_RESAMPLE.H>
#include <string.h>

/* ── ResampleAndAddToMix ─────────────────────────────────────────────────── */

static void test_resample_same_rate_16bit_add(void) {
    short src[4] = {100, 200, -100, -200};
    short mix[4] = {0, 0, 0, 0};
    ResampleAndAddToMix(22050, 22050, src, 8, mix, 4, 1);
    ASSERT_EQ_INT(100, mix[0]);
    ASSERT_EQ_INT(200, mix[1]);
    ASSERT_EQ_INT(-100, mix[2]);
    ASSERT_EQ_INT(-200, mix[3]);
}

static void test_resample_same_rate_16bit_clip(void) {
    short src[2] = {20000, -20000};
    short mix[2] = {20000, -20000};
    ResampleAndAddToMix(22050, 22050, src, 4, mix, 2, 1);
    ASSERT_EQ_INT(32767, mix[0]);  /* 20000+20000 clips high */
    ASSERT_EQ_INT(-32768, mix[1]); /* -20000-20000 clips low */
}

static void test_resample_same_rate_8bit(void) {
    U8 src[4] = {128, 200, 128, 50};
    U8 mix[4] = {128, 128, 128, 128};
    ResampleAndAddToMix(22050, 22050, src, 4, mix, 4, 0);
    ASSERT_EQ_INT(128, mix[0]);
    ASSERT_EQ_INT(200, mix[1]);
    ASSERT_EQ_INT(128, mix[2]);
    ASSERT_EQ_INT(50, mix[3]);
}

static void test_resample_8bit_clip(void) {
    U8 src[2] = {200, 50};
    U8 mix[2] = {200, 0};
    ResampleAndAddToMix(22050, 22050, src, 2, mix, 2, 0);
    ASSERT_EQ_INT(255, mix[0]); /* 72+72+128=272 -> clip to 255 */
    ASSERT_EQ_INT(0, mix[1]);   /* -128-78+128=-78 -> clip to -128 -> 0 */
}

static void test_resample_upsample_16bit(void) {
    short src[2] = {1000, 2000};
    short mix[4] = {0, 0, 0, 0};
    ResampleAndAddToMix(11025, 22050, src, 4, mix, 4, 1);
    ASSERT_TRUE(mix[0] >= 500 && mix[0] <= 1500);
    ASSERT_TRUE(mix[3] >= 1500 && mix[3] <= 2500);
}

static void test_resample_zero_input_noop(void) {
    short mix[4] = {42, 42, 42, 42};
    ResampleAndAddToMix(22050, 22050, (const void *)"", 0, mix, 4, 1);
    ASSERT_EQ_INT(42, mix[0]);
}

static void test_resample_zero_rate_noop(void) {
    short src[2] = {100, 200};
    short mix[2] = {99, 99};
    ResampleAndAddToMix(0, 22050, src, 4, mix, 2, 1);
    ASSERT_EQ_INT(99, mix[0]);
}

/* ── GetEffectiveTrackRate ────────────────────────────────────────────────── */

static void test_effective_rate_track1_mono_22050(void) {
    unsigned long a_r[7] = {22050, 22050, 22050, 22050, 0, 0, 0};
    U8 a_c[7] = {2, 1, 1, 1, 0, 0, 0};
    unsigned long r = GetEffectiveTrackRate(1, a_r, a_c, 22050);
    ASSERT_EQ_INT(11025, (int)r);
}

static void test_effective_rate_track0_unchanged(void) {
    unsigned long a_r[7] = {22050, 22050, 22050, 22050, 0, 0, 0};
    U8 a_c[7] = {2, 1, 1, 1, 0, 0, 0};
    unsigned long r = GetEffectiveTrackRate(0, a_r, a_c, 22050);
    ASSERT_EQ_INT(22050, (int)r);
}

static void test_effective_rate_stereo_unchanged(void) {
    unsigned long a_r[7] = {22050, 22050, 0, 0, 0, 0, 0};
    U8 a_c[7] = {2, 2, 0, 0, 0, 0, 0};
    unsigned long r = GetEffectiveTrackRate(1, a_r, a_c, 22050);
    ASSERT_EQ_INT(22050, (int)r);
}

static void test_effective_rate_track4_unchanged(void) {
    unsigned long a_r[7] = {22050, 22050, 22050, 22050, 11025, 0, 0};
    U8 a_c[7] = {2, 1, 1, 1, 1, 0, 0};
    unsigned long r = GetEffectiveTrackRate(4, a_r, a_c, 22050);
    ASSERT_EQ_INT(11025, (int)r);
}

int main(void) {
    RUN_TEST(test_resample_same_rate_16bit_add);
    RUN_TEST(test_resample_same_rate_16bit_clip);
    RUN_TEST(test_resample_same_rate_8bit);
    RUN_TEST(test_resample_8bit_clip);
    RUN_TEST(test_resample_upsample_16bit);
    RUN_TEST(test_resample_zero_input_noop);
    RUN_TEST(test_resample_zero_rate_noop);
    RUN_TEST(test_effective_rate_track1_mono_22050);
    RUN_TEST(test_effective_rate_track0_unchanged);
    RUN_TEST(test_effective_rate_stereo_unchanged);
    RUN_TEST(test_effective_rate_track4_unchanged);
    TEST_SUMMARY();
    return test_failures != 0;
}
