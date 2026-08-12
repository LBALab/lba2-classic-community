/* Host regression test for CD audio byte order (LIB386/SYSTEM/CDDA.CPP).
 *
 * Red Book stores samples MSB-first and a ripper is meant to swap them. Some do
 * not, and a track played in the wrong order is not subtly wrong, it is uniform
 * full-scale noise. Nothing errors when this rule misfires, which is exactly why
 * it is pinned: a needless swap would turn every working disc's theme into noise
 * just as surely as a missed one leaves the broken disc broken.
 *
 * The signals here are built to the shape of the real measurements. Music has
 * crest factor and averages a few per cent of full scale; bytes in the wrong
 * order are uniform noise averaging about half of it.
 */
#include "test_harness.h"

#include <SYSTEM/CDDA.H>

#include <stdint.h>
#include <string.h>
#include <vector>

/* A deterministic LCG, so "noise" is the same on every machine and every run. */
static uint32_t lcg(uint32_t *state) {
    *state = (*state * 1103515245u + 12345u) & 0x7FFFFFFFu;
    return *state;
}

/* Uniform noise: what a data track, or audio in the wrong byte order, looks
   like. Mean magnitude lands near half of full scale. */
static std::vector<uint8_t> noise_buffer(size_t bytes) {
    std::vector<uint8_t> b(bytes);
    uint32_t s = 12345;
    for (size_t i = 0; i < bytes; i++)
        b[i] = (uint8_t)(lcg(&s) >> 8);
    return b;
}

/* A quiet triangle wave, standing in for music: a real waveform whose mean
   magnitude is a few per cent of full scale. Written little-endian. */
static std::vector<uint8_t> music_buffer(size_t bytes, int amplitude) {
    std::vector<uint8_t> b(bytes, 0);
    int v = 0, step = amplitude / 64 + 1;
    for (size_t i = 0; i + 1 < bytes; i += 2) {
        v += step;
        if (v > amplitude || v < -amplitude)
            step = -step;
        b[i] = (uint8_t)(v & 0xFF);
        b[i + 1] = (uint8_t)((v >> 8) & 0xFF);
    }
    return b;
}

static std::vector<uint8_t> swapped(const std::vector<uint8_t> &in) {
    std::vector<uint8_t> out(in);
    CdDa_ByteSwap(out.data(), out.size());
    return out;
}

#define BUF (256 * 1024)

/* Audio already in the right order must be left alone. This is the direction
   that matters most: every disc that works today takes it. */
static void test_correct_order_is_left_alone(void) {
    std::vector<uint8_t> music = music_buffer(BUF, 3000);
    ASSERT_EQ_INT(0, CdDa_NeedsByteSwap(music.data(), music.size()));
}

/* The same music byte-swapped reads as noise, and has to be swapped back. */
static void test_swapped_music_is_detected(void) {
    std::vector<uint8_t> wrong = swapped(music_buffer(BUF, 3000));
    ASSERT_EQ_INT(1, CdDa_NeedsByteSwap(wrong.data(), wrong.size()));
}

/* Round trip: swapping what the rule flags produces something it no longer
   flags, so applying it twice cannot oscillate. */
static void test_swap_settles(void) {
    std::vector<uint8_t> wrong = swapped(music_buffer(BUF, 3000));
    ASSERT_EQ_INT(1, CdDa_NeedsByteSwap(wrong.data(), wrong.size()));
    CdDa_ByteSwap(wrong.data(), wrong.size());
    ASSERT_EQ_INT(0, CdDa_NeedsByteSwap(wrong.data(), wrong.size()));
}

/* Loud music is still music. The rule keys on distribution, not level, so a
   track mastered hot must not be mistaken for bytes in the wrong order. */
static void test_loud_music_is_not_swapped(void) {
    std::vector<uint8_t> loud = music_buffer(BUF, 30000);
    ASSERT_EQ_INT(0, CdDa_NeedsByteSwap(loud.data(), loud.size()));
}

/* Silence carries no evidence either way, and neither does a buffer too short
   to sample. The safe answer is to leave the audio alone. */
static void test_silence_and_short_buffers_do_nothing(void) {
    std::vector<uint8_t> quiet(BUF, 0);
    ASSERT_EQ_INT(0, CdDa_NeedsByteSwap(quiet.data(), quiet.size()));

    std::vector<uint8_t> tiny = noise_buffer(64);
    ASSERT_EQ_INT(0, CdDa_NeedsByteSwap(tiny.data(), tiny.size()));
    ASSERT_EQ_INT(0, CdDa_NeedsByteSwap(NULL, BUF));
}

/* Uniform noise is symmetric: it reads the same magnitude whichever way round
   the bytes go, so neither order wins and nothing is swapped. A data track
   mistaken for audio must not be "fixed" into a different arrangement of noise. */
static void test_noise_has_no_preferred_order(void) {
    std::vector<uint8_t> n = noise_buffer(BUF);
    ASSERT_EQ_INT(0, CdDa_NeedsByteSwap(n.data(), n.size()));
}

/* The swap itself: byte pairs exchange, and an odd trailing byte is untouched
   rather than read past. */
static void test_byte_swap_mechanics(void) {
    uint8_t buf[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
    CdDa_ByteSwap(buf, sizeof(buf));
    ASSERT_EQ_INT(0x22, buf[0]);
    ASSERT_EQ_INT(0x11, buf[1]);
    ASSERT_EQ_INT(0x44, buf[2]);
    ASSERT_EQ_INT(0x33, buf[3]);
    ASSERT_EQ_INT(0x55, buf[4]);
    CdDa_ByteSwap(NULL, 16); /* must not crash */
}

int main(void) {
    RUN_TEST(test_correct_order_is_left_alone);
    RUN_TEST(test_swapped_music_is_detected);
    RUN_TEST(test_swap_settles);
    RUN_TEST(test_loud_music_is_not_swapped);
    RUN_TEST(test_silence_and_short_buffers_do_nothing);
    RUN_TEST(test_noise_has_no_preferred_order);
    RUN_TEST(test_byte_swap_mechanics);
    TEST_SUMMARY();
    return test_failures != 0;
}
