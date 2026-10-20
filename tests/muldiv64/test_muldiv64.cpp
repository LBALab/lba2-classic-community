/*
 * MULDIV64 must multiply in 64 bits, as the original `imul edx; idiv ecx` did.
 *
 * Its only caller is the diagonal terrain slide in DoCornerReajustExtPt
 * (SOURCES/EXTFUNC.CPP), which moves the hero along a diagonal edge by
 * MULDIV64(d, SinTab[...] * 65536, SQR_2). The multiplier is up to 2^30, so a
 * 32-bit product wraps for any step of 2 units or more and the slide comes out
 * as -1, 0 or 1: the hero stops on the edge instead of sliding along it.
 *
 * The pinned values are d / sqrt(2) truncated, the slide for a 45 degree
 * approach, worked out by hand. A 64-bit `imul` followed by `idiv`, the
 * instructions the 1997 DOS game runs here, gives the same numbers.
 */

#include "test_harness.h"

#include <SYSTEM.H>

#include <stdio.h>

#define SQR_2 1518500250 /* sqrt(2) << 30, the divisor SOURCES/EXTFUNC.CPP passes */

static void test_slide_at_45_degrees() {
    const S32 mul = 16384 * 65536; /* SinTab[1024], cos(0), so 2^30 */
    ASSERT_EQ_INT(5, MULDIV64(8, mul, SQR_2));
    ASSERT_EQ_INT(11, MULDIV64(16, mul, SQR_2));
    ASSERT_EQ_INT(22, MULDIV64(32, mul, SQR_2));
    ASSERT_EQ_INT(45, MULDIV64(64, mul, SQR_2));
    ASSERT_EQ_INT(494, MULDIV64(700, mul, SQR_2));
    /* Sens 1 and the far side give negative multipliers; idiv truncates toward zero. */
    ASSERT_EQ_INT(-45, MULDIV64(64, -mul, SQR_2));
}

static S64 Abs64(S64 v) { return v < 0 ? -v : v; }

/* q is p / div truncated toward zero: |q| * div <= |p| < (|q| + 1) * div, and
 * q has p's sign. This checks the definition, not a copy of the code. */
static bool IsTruncatedQuotient(S64 p, S64 div, S32 q) {
    const S64 aq = Abs64(q);
    if (q != 0 && (q < 0) != (p < 0))
        return false;
    return aq * div <= Abs64(p) && Abs64(p) < (aq + 1) * div;
}

/* Every value SinTab can hold (-16384..16384), not only the table's entries,
 * times every step length up to 2048. */
static void test_every_sine_gives_the_truncated_quotient() {
    int mismatches = 0;
    for (S32 sine = -16384; sine <= 16384; sine++) {
        const S32 mul = sine * 65536;
        for (S32 d = 0; d <= 2048; d++) {
            const S32 got = MULDIV64(d, mul, SQR_2);
            if (!IsTruncatedQuotient((S64)d * mul, SQR_2, got) && mismatches++ < 5) {
                printf("    sine %d d %d: got %d\n", (int)sine, (int)d, (int)got);
            }
        }
    }
    ASSERT_EQ_INT(0, mismatches);
}

int main() {
    RUN_TEST(test_slide_at_45_degrees);
    RUN_TEST(test_every_sine_gives_the_truncated_quotient);
    TEST_SUMMARY();
    return test_failures ? 1 : 0;
}
