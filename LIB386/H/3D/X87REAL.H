#pragma once

/*
 * The arithmetic type of the float 3D routines (the F files in LIB386/3D).
 *
 * The 1997 routines run on the x87: 80-bit registers through the fmul/fadd
 * chain, then fistp, which rounds to nearest with ties to even. The C ports
 * write their expressions in X87_REAL and round them with X87Round.
 *
 * Where long double is the x87's 80-bit format (x86-64 Linux and MinGW, 32-bit
 * x86), X87_REAL is long double and X87Round is lrintl, which is the x87 itself.
 *
 * Everywhere else X87_REAL is double. A long double there is no closer to the
 * x87: it is a double already (Apple, MSVC, 32-bit ARM), or a 113-bit or
 * double-double type done in software (64-bit Android and Linux on ARM,
 * PowerPC), which made a body point cost about 100 times more on a phone and
 * still rounded the same results. Measured over recorded game inputs, double
 * gives the x87's results for the rotations and the lighting (the products are
 * exact in double) and differs only at exact half-pixel ties in the
 * projections, about one value in 100,000, by one pixel; 113-bit and
 * double-double miss the same ties.
 *
 * The test is on the type, not the platform: MSVC's long double is a double,
 * and 32-bit x86's is 80-bit.
 *
 * The double branch rounds inline instead of through lrint, which is a library
 * call on 32-bit ARM. Adding 1.5 * 2^52 rounds v to an integer by the current
 * rounding mode, which is round-to-nearest-even here:
 * the only change of mode is truncation inside one triangle's span in
 * pol_work/POLY.CPP, which these routines never run inside. It needs |v| < 2^51,
 * far above anything that still fits the S32 the callers store. Everything is
 * built with -ffp-contract=off (the top CMakeLists.txt): GCC otherwise fuses a
 * caller's multiply into the first addition, which is one rounding instead of
 * two and moves ties.
 */

#include <float.h>
#include <math.h>
#include <string.h>
#include <SYSTEM/ADELINE_TYPES.H>

// Round to nearest, ties to even, without a library call. Only where double
// arithmetic is evaluated in double: with x87 excess precision (FLT_EVAL_METHOD
// 2) the add would round at 64 bits and the trick fails. Defined apart from
// X87Round so x86 hosts, which use lrintl, can still test it. FLT_EVAL_METHOD
// is C99 and C++11, and the library builds as C++98, so the compiler's own
// macro comes first.
#if defined(__FLT_EVAL_METHOD__)
#define X87_FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#elif defined(FLT_EVAL_METHOD)
#define X87_FLT_EVAL_METHOD FLT_EVAL_METHOD
#elif defined(_M_X64) || defined(_M_ARM64) || defined(_M_ARM)
#define X87_FLT_EVAL_METHOD 0
#endif

#if defined(X87_FLT_EVAL_METHOD) && X87_FLT_EVAL_METHOD == 0
#define X87_HAVE_ROUND_DOUBLE 1
static inline S32 X87RoundDouble(double v) {
    // v + 1.5 * 2^52 lies in [2^52, 2^53), where the step is 1, so the add
    // rounds v to an integer, and the low 32 bits of the sum's mantissa are
    // that integer modulo 2^32 (the constant's are zero, and the bit above
    // absorbs a negative's borrow). That is lrintl's result cast to S32 on
    // x86-64, read without a double-to-integer conversion, which on 32-bit
    // ARM is a library call for 64-bit integers.
    const double t = v + 6755399441055744.0;
    U64 bits;
    memcpy(&bits, &t, sizeof(bits));
    return (S32)(U32)bits;
}
#endif

#if LDBL_MANT_DIG == 64

typedef long double X87_REAL;
#define X87_REAL_MANT_DIG LDBL_MANT_DIG

static inline S32 X87Round(X87_REAL v) {
    return (S32)lrintl(v);
}

#else

#ifndef X87_HAVE_ROUND_DOUBLE
#error "X87REAL.H: double must evaluate as double here (FLT_EVAL_METHOD 0)"
#endif

typedef double X87_REAL;
#define X87_REAL_MANT_DIG DBL_MANT_DIG

static inline S32 X87Round(X87_REAL v) {
    return X87RoundDouble(v);
}

#endif
