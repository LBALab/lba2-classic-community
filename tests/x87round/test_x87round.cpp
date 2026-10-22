/* Host test for 3D/X87REAL.H, the type and rounding of the float 3D routines.
 *
 * X87RoundDouble is the inline rounding used where long double is not the x87's
 * 80-bit type (ARM, PowerPC, MSVC). It has to give what fistp gives in
 * round-to-nearest mode, which is what llrint gives: ties to even, both signs,
 * up to the 2^51 bound. X87Round is checked the same way on whichever branch
 * this host takes.
 *
 * The last test is for the -ffp-contract=off flag the build sets (CMakeLists.txt).
 * A product that rounds to an exact half in double, but is not exactly a half,
 * rounds to even only if the multiply and the add are rounded separately. With
 * them fused (GCC's default where the CPU has an FMA) it goes the other way.
 *
 * X87Fistp is the rounding of the polygon fillers, which draw each triangle
 * under truncation: it has to follow the current mode as fistp does, where the
 * inline rounding would floor a negative value instead of truncating it. */

#include "test_harness.h"

#include <3D/X87REAL.H>

#include <fenv.h>
#include <math.h>
#include <stdio.h>

static U32 rng_state = 0x808808u;
static U32 Next() {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state;
}

static void CheckBoth(double v) {
    const S32 want = (S32)llrint(v);
#ifdef X87_HAVE_ROUND_DOUBLE
    ASSERT_EQ_INT(want, X87RoundDouble(v));
#endif
    ASSERT_EQ_INT(want, X87Round((X87_REAL)v));
}

static void test_ties_go_to_even() {
    for (int k = -6; k <= 6; k++) {
        const double tie = k + 0.5;
        const S32 even = (k % 2 == 0) ? k : k + 1;
        CheckBoth(tie);
        ASSERT_EQ_INT(even, (S32)llrint(tie));
    }
    CheckBoth(2047.5);
    CheckBoth(-2047.5);
    CheckBoth(32766.5);
    CheckBoth(-32767.5);
}

static void test_zero_and_near_ties() {
    CheckBoth(0.0);
    CheckBoth(-0.0);
    CheckBoth(0.49999999999999994);
    CheckBoth(-0.49999999999999994);
    CheckBoth(0.5000000000000001);
    CheckBoth(-0.5000000000000001);
    CheckBoth(1.4999999999999998);
    CheckBoth(nextafter(2.5, 3.0));
    CheckBoth(nextafter(-2.5, -3.0));
}

static void test_large_values() {
    // Inside S32, on both branches.
    const double inside[] = {2147483647.0, -2147483648.0, 2147483646.5, -2147483647.5, 1073741824.5, -1073741823.5};
    for (double v : inside) {
        CheckBoth(v);
    }

    // Beyond S32, up to the bound: the inline rounding gives the low 32 bits, as
    // lrintl's 64-bit long cast to S32 does on x86-64. Not asked of X87Round:
    // where long is 32 bits (32-bit x86) lrintl gives INT_MIN there instead, as
    // the 1997 fistp to a 32-bit integer does. The routines never get there.
#ifdef X87_HAVE_ROUND_DOUBLE
    const double beyond[] = {4294967296.5, 1125899906842623.5, -1125899906842623.5, 2251799813685247.0,
                             -2251799813685247.0};
    for (double v : beyond) {
        ASSERT_EQ_INT((S32)llrint(v), X87RoundDouble(v));
    }
#endif
}

static void test_random_values() {
    // Halves and quarters of random integers, then arbitrary fractions.
    for (int i = 0; i < 200000; i++) {
        const S32 n = (S32)Next();
        CheckBoth(n / 2.0);
        CheckBoth(n / 4.0);
        CheckBoth((double)(S32)Next() / (double)((Next() >> 8) | 1));
    }
}

#ifdef X87_HAVE_ROUND_DOUBLE
// Out of line, so the product is computed here and not shared with the
// caller's copy of it, which would leave nothing to fuse.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static S32
RoundProduct(double x, double f) {
    return X87RoundDouble(x * f);
}
#endif

static void test_multiply_is_not_fused() {
#ifdef X87_HAVE_ROUND_DOUBLE
    int traps = 0;
    int fused = 0;
    for (int i = 0; i < 200000 && traps < 2000; i++) {
        const double x = (double)(S32)((Next() >> 12) | 1);
        const double k = (double)(S32)(Next() >> 20) - 2048.0;
        const double f = (k + 0.5) / x;
        volatile double product = x * f;
        if (product != k + 0.5 || fma(x, f, -product) == 0.0)
            continue; // not a rounded-to-half product
        traps++;
        const S32 want = (S32)llrint(product);
        const S32 got = RoundProduct(x, f);
        if (got != want && fused++ < 3)
            printf("  x=%.17g f=%.17g: got %d, want %d\n", x, f, (int)got, (int)want);
    }
    printf("  %d products that round to an exact half, %d rounded as if fused\n", traps, fused);
    ASSERT_TRUE(traps > 100);
    ASSERT_EQ_INT(0, fused);
#endif
}

static void test_fistp_follows_the_mode() {
    // volatile, so no rounding is folded at compile time in the default mode.
    volatile double values[] = {0.0, -0.0, 0.25, -0.25, 2.5, -2.5, 3.5, -3.5, 7.99999, -7.99999,
                                -9471.000001, 155477.75, -155477.75, 2147483646.5, -2147483647.5};
    for (double v : values) {
        ASSERT_EQ_INT((S32)llrint(v), X87Fistp((X87_REAL)v));
    }
    for (int i = 0; i < 100000; i++) {
        volatile double v = (double)(S32)Next() / (double)((Next() >> 8) | 1);
        ASSERT_EQ_INT((S32)llrint(v), X87Fistp((X87_REAL)v));
    }

    int floored = 0;
    fesetround(FE_TOWARDZERO);
    for (double v : values) {
        ASSERT_EQ_INT((S32)v, X87Fistp((X87_REAL)v));
    }
    for (int i = 0; i < 100000; i++) {
        volatile double v = (double)(S32)Next() / (double)((Next() >> 8) | 1);
        ASSERT_EQ_INT((S32)v, X87Fistp((X87_REAL)v));
#ifdef X87_HAVE_ROUND_DOUBLE
        if (X87RoundDouble(v) != (S32)v)
            floored++;
#endif
    }
    fesetround(FE_TONEAREST);
#ifdef X87_HAVE_ROUND_DOUBLE
    // Why the fillers do not use X87Round: under truncation the inline rounding
    // floors the negative values.
    printf("  under truncation, the inline rounding missed %d of 100000\n", floored);
    ASSERT_TRUE(floored > 10000);
#endif
}

int main() {
    printf("X87REAL.H: long double has %d mantissa bits, X87_REAL is %s\n", (int)LDBL_MANT_DIG,
           sizeof(X87_REAL) == sizeof(double) ? "double" : "long double");
    RUN_TEST(test_ties_go_to_even);
    RUN_TEST(test_zero_and_near_ties);
    RUN_TEST(test_large_values);
    RUN_TEST(test_random_values);
    RUN_TEST(test_multiply_is_not_fused);
    RUN_TEST(test_fistp_follows_the_mode);
    TEST_SUMMARY();
    return test_failures != 0;
}
