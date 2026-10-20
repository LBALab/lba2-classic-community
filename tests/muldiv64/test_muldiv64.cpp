/*
 * MULDIV64 must multiply in 64 bits, as the original `imul edx; idiv ecx` did.
 *
 * Its only caller is the diagonal terrain slide in DoCornerReajustExtPt
 * (SOURCES/EXTFUNC.CPP), which moves the hero along a diagonal edge by
 * MULDIV64(d, SinTab[...] << 16, SQR_2). The multiplier is up to 2^30, so a
 * 32-bit product wraps for any step of 2 units or more and the slide comes out
 * as -1, 0 or 1: the hero stops on the edge instead of sliding along it.
 *
 * The pinned values are d / sqrt(2), the slide for a 45 degree approach, worked
 * out by hand; they are what the 1997 DOS game computes.
 */

#include "test_harness.h"

#include <SYSTEM.H>

#include <stdio.h>

#define SQR_2 1518500250 /* sqr(2)<<30, as in SOURCES/EXTFUNC.CPP */

static void test_slide_at_45_degrees() {
    const S32 mul = 16384 << 16; /* SinTab[1024], cos(0), so 2^30 */
    ASSERT_EQ_INT(5, MULDIV64(8, mul, SQR_2));
    ASSERT_EQ_INT(11, MULDIV64(16, mul, SQR_2));
    ASSERT_EQ_INT(22, MULDIV64(32, mul, SQR_2));
    ASSERT_EQ_INT(45, MULDIV64(64, mul, SQR_2));
    ASSERT_EQ_INT(494, MULDIV64(700, mul, SQR_2));
    /* Sens 1 and the far side give negative multipliers; idiv truncates toward zero. */
    ASSERT_EQ_INT(-45, MULDIV64(64, -mul, SQR_2));
}

/* Every value SinTab can hold (-16384..16384), not only the table's entries. */
static void test_every_sine_matches_a_64_bit_product() {
    int mismatches = 0;
    for (S32 sine = -16384; sine <= 16384; sine += 7) {
        const S32 mul = sine * 65536;
        for (S32 d = 0; d <= 2048; d++) {
            const S32 want = (S32)(((S64)d * mul) / SQR_2);
            if (MULDIV64(d, mul, SQR_2) != want && mismatches++ < 5) {
                printf("    sine %d d %d: got %d, want %d\n", (int)sine, (int)d,
                       (int)MULDIV64(d, mul, SQR_2), (int)want);
            }
        }
    }
    ASSERT_EQ_INT(0, mismatches);
}

int main() {
    RUN_TEST(test_slide_at_45_degrees);
    RUN_TEST(test_every_sine_matches_a_64_bit_product);
    TEST_SUMMARY();
    return test_failures ? 1 : 0;
}
