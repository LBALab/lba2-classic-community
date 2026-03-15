/*
 * test_fpu_precision.cpp — FPU precision comparison: x87 ASM vs portable C approaches
 *
 * For each common x87 FPU pattern used in the game, this test:
 *   1. Calls the ASM reference (asm_fpu_*) to get the ground truth
 *   2. Calls multiple C implementations of the same operation
 *   3. Reports which C approach matches / how far off each one is
 *
 * The goal is NOT to pass/fail, but to measure precision gaps and identify
 * the best portable C strategy for each FPU pattern.
 */

#include "test_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

/* ── ASM reference functions (from fpu_ops.asm, renamed via objcopy) ── */
extern "C" {
    int32_t asm_fpu_fild_fistp_round(int32_t val);
    int32_t asm_fpu_int_div_int(int32_t a, int32_t b);
    int32_t asm_fpu_int_div_int_trunc(int32_t a, int32_t b);
    int32_t asm_fpu_reciprocal_mul(int32_t w, int32_t val);
    int32_t asm_fpu_reciprocal_mul_trunc(int32_t w, int32_t val);
    int32_t asm_fpu_slope(int32_t dx, int32_t dy);
    int32_t asm_fpu_slope_trunc(int32_t dx, int32_t dy);
    int32_t asm_fpu_interp_factor(int32_t zclip, int32_t lastZ, int32_t curZ,
                                   int32_t lastVal, int32_t curVal);
    void    asm_fpu_mul_add_chain(float a, float b, float c, float d, float *out);
    void    asm_fpu_mul_sub_chain(float a, float b, float c, float d, float *out);
    int32_t asm_fpu_float_to_int_round(float val);
    int32_t asm_fpu_float_to_int_trunc(float val);
    void    asm_fpu_perspective_uv(int32_t w, int32_t u, int32_t v,
                                    int32_t *out_u, int32_t *out_v);
    void    asm_fpu_dot3(float *a, float *b, float *out);
    int32_t asm_fpu_reciprocal_mul_256(int32_t w, int32_t val);
}

/* ── Deterministic LCG RNG ─────────────────────────────────────────── */
static uint32_t rng_state;
static void rng_seed(uint32_t s) { rng_state = s; }
static uint32_t rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}
static int32_t rng_range(int32_t lo, int32_t hi) {
    return lo + (int32_t)(rng_next() % (uint32_t)(hi - lo + 1));
}

/* ── Counters for summary ──────────────────────────────────────────── */
static int total_comparisons = 0;
static int total_matches     = 0;
static int total_mismatches  = 0;

/* ── Report helper ─────────────────────────────────────────────────── */
static void report_int_comparison(const char *op_name, const char *approach,
                                   int32_t asm_val, int32_t cpp_val,
                                   const char *input_desc) {
    total_comparisons++;
    int32_t diff = asm_val - cpp_val;
    if (diff == 0) {
        total_matches++;
    } else {
        total_mismatches++;
        printf("    DIFF [%s] %s: ASM=%d CPP=%d diff=%d  (%s)\n",
               op_name, approach, asm_val, cpp_val, diff, input_desc);
    }
}

static void report_float_comparison(const char *op_name, const char *approach,
                                     float asm_val, float cpp_val,
                                     const char *input_desc) {
    total_comparisons++;
    /* Bit-exact comparison */
    uint32_t asm_bits, cpp_bits;
    memcpy(&asm_bits, &asm_val, 4);
    memcpy(&cpp_bits, &cpp_val, 4);
    if (asm_bits == cpp_bits) {
        total_matches++;
    } else {
        total_mismatches++;
        float diff = asm_val - cpp_val;
        printf("    DIFF [%s] %s: ASM=%.10e (0x%08X) CPP=%.10e (0x%08X) diff=%.10e  (%s)\n",
               op_name, approach, asm_val, asm_bits, cpp_val, cpp_bits,
               diff, input_desc);
    }
}

/* ====================================================================
 * C approach implementations for each FPU pattern
 * ==================================================================== */

/* --- int div int (round to nearest) --- */
static int32_t c_int_div_int_double(int32_t a, int32_t b) {
    return (int32_t)lrint((double)a / (double)b);
}
static int32_t c_int_div_int_longdouble(int32_t a, int32_t b) {
    return (int32_t)lrintl((long double)a / (long double)b);
}
static int32_t c_int_div_int_volatile_ld(int32_t a, int32_t b) {
    volatile long double va = (long double)a;
    volatile long double vb = (long double)b;
    volatile long double r = va / vb;
    return (int32_t)lrintl(r);
}

/* --- int div int (truncation) --- */
static int32_t c_int_div_int_trunc_cast(int32_t a, int32_t b) {
    return (int32_t)((double)a / (double)b);
}
static int32_t c_int_div_int_trunc_ld(int32_t a, int32_t b) {
    return (int32_t)((long double)a / (long double)b);
}
static int32_t c_int_div_int_trunc_volatile_ld(int32_t a, int32_t b) {
    volatile long double va = (long double)a;
    volatile long double vb = (long double)b;
    volatile long double r = va / vb;
    return (int32_t)r;
}

/* --- reciprocal mul: 256.0/w * val (round-to-nearest) --- */
static int32_t c_recip_mul_double(int32_t w, int32_t val) {
    return (int32_t)lrint(256.0 / (double)w * (double)val);
}
static int32_t c_recip_mul_longdouble(int32_t w, int32_t val) {
    return (int32_t)lrintl(256.0L / (long double)w * (long double)val);
}
static int32_t c_recip_mul_volatile_ld(int32_t w, int32_t val) {
    volatile long double vw = (long double)w;
    volatile long double invW = 256.0L / vw;
    volatile long double result = invW * (long double)val;
    return (int32_t)lrintl(result);
}
static int32_t c_recip_mul_float(int32_t w, int32_t val) {
    return (int32_t)lrintf(256.0f / (float)w * (float)val);
}
static int32_t c_recip_mul_staged_ld(int32_t w, int32_t val) {
    /* Stage 1: fild w → 80-bit, then 256.0f (loaded as float) / w */
    volatile long double vw = (long double)w;
    volatile long double invW = (long double)256.0f / vw;
    /* Stage 2: fild val, multiply by invW */
    volatile long double vval = (long double)val;
    volatile long double result = vval * invW;
    return (int32_t)lrintl(result);
}

/* --- slope: dx/dy + 1.0 (round-to-nearest) --- */
static int32_t c_slope_double(int32_t dx, int32_t dy) {
    return (int32_t)lrint((double)dx / (double)dy + 1.0);
}
static int32_t c_slope_longdouble(int32_t dx, int32_t dy) {
    return (int32_t)lrintl((long double)dx / (long double)dy + 1.0L);
}
static int32_t c_slope_volatile_ld(int32_t dx, int32_t dy) {
    volatile long double vdy = (long double)dy;
    volatile long double inv = 1.0L / vdy;
    volatile long double vdx = (long double)dx;
    volatile long double r = vdx * inv;
    volatile long double r2 = r + 1.0L;
    return (int32_t)lrintl(r2);
}
static int32_t c_slope_staged_ld(int32_t dx, int32_t dy) {
    /* Match ASM order: fild dy / fdivr F_1 / fild dx / fmulp / fadd F_1 / fistp */
    volatile long double vdy = (long double)dy;
    volatile long double inv_dy = (long double)1.0f / vdy; /* fdivr F_1 (F_1 is float 1.0) */
    volatile long double vdx = (long double)dx;
    volatile long double product = vdx * inv_dy;            /* fmulp */
    volatile long double result = product + (long double)1.0f; /* fadd F_1 */
    return (int32_t)lrintl(result);
}

/* --- slope truncation mode --- */
static int32_t c_slope_trunc_double(int32_t dx, int32_t dy) {
    return (int32_t)((double)dx / (double)dy + 1.0);
}
static int32_t c_slope_trunc_longdouble(int32_t dx, int32_t dy) {
    return (int32_t)((long double)dx / (long double)dy + 1.0L);
}
static int32_t c_slope_trunc_volatile_ld(int32_t dx, int32_t dy) {
    volatile long double vdy = (long double)dy;
    volatile long double inv = 1.0L / vdy;
    volatile long double vdx = (long double)dx;
    volatile long double r = vdx * inv;
    volatile long double r2 = r + 1.0L;
    return (int32_t)r2;
}
static int32_t c_slope_trunc_staged_ld(int32_t dx, int32_t dy) {
    volatile long double vdy = (long double)dy;
    volatile long double inv_dy = (long double)1.0f / vdy;
    volatile long double vdx = (long double)dx;
    volatile long double product = vdx * inv_dy;
    volatile long double result = product + (long double)1.0f;
    return (int32_t)result;
}

/* --- interpolation factor --- */
static int32_t c_interp_double(int32_t zclip, int32_t lastZ, int32_t curZ,
                                int32_t lastVal, int32_t curVal) {
    double factor = (double)(zclip - lastZ) / (double)(curZ - lastZ);
    return (int32_t)lrint((double)lastVal + (double)(curVal - lastVal) * factor);
}
static int32_t c_interp_longdouble(int32_t zclip, int32_t lastZ, int32_t curZ,
                                    int32_t lastVal, int32_t curVal) {
    long double factor = (long double)(zclip - lastZ) / (long double)(curZ - lastZ);
    return (int32_t)lrintl((long double)lastVal + (long double)(curVal - lastVal) * factor);
}
static int32_t c_interp_volatile_ld(int32_t zclip, int32_t lastZ, int32_t curZ,
                                     int32_t lastVal, int32_t curVal) {
    volatile long double num = (long double)(zclip - lastZ);
    volatile long double den = (long double)(curZ - lastZ);
    volatile long double factor = num / den;
    volatile long double diff_val = (long double)(curVal - lastVal);
    volatile long double product = diff_val * factor;
    volatile long double lv = (long double)lastVal;
    volatile long double result = lv + product;
    return (int32_t)lrintl(result);
}

/* --- float to int (round-to-nearest) --- */
static int32_t c_ftoi_round_lrint(float val) {
    return (int32_t)lrintf(val);
}
static int32_t c_ftoi_round_lrintl_ld(float val) {
    return (int32_t)lrintl((long double)val);
}

/* --- float to int (truncation) --- */
static int32_t c_ftoi_trunc_cast(float val) {
    return (int32_t)val;
}

/* --- float mul-add chain: a*b + c*d --- */
static float c_mul_add_float(float a, float b, float c, float d) {
    return a * b + c * d;
}
static float c_mul_add_double(float a, float b, float c, float d) {
    return (float)((double)a * (double)b + (double)c * (double)d);
}
static float c_mul_add_longdouble(float a, float b, float c, float d) {
    return (float)((long double)a * (long double)b + (long double)c * (long double)d);
}
static float c_mul_add_volatile_ld(float a, float b, float c, float d) {
    volatile long double va = (long double)a;
    volatile long double vb = (long double)b;
    volatile long double ab = va * vb;
    volatile long double vc = (long double)c;
    volatile long double vd = (long double)d;
    volatile long double cd = vc * vd;
    volatile long double r = ab + cd;
    return (float)r;
}

/* --- float mul-sub chain: a*b - c*d --- */
static float c_mul_sub_float(float a, float b, float c, float d) {
    return a * b - c * d;
}
static float c_mul_sub_double(float a, float b, float c, float d) {
    return (float)((double)a * (double)b - (double)c * (double)d);
}
static float c_mul_sub_longdouble(float a, float b, float c, float d) {
    return (float)((long double)a * (long double)b - (long double)c * (long double)d);
}

/* --- dot product --- */
static float c_dot3_float(float *a, float *b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
static float c_dot3_double(float *a, float *b) {
    return (float)((double)a[0]*(double)b[0] + (double)a[1]*(double)b[1] + (double)a[2]*(double)b[2]);
}
static float c_dot3_longdouble(float *a, float *b) {
    return (float)((long double)a[0]*(long double)b[0] + (long double)a[1]*(long double)b[1] + (long double)a[2]*(long double)b[2]);
}

/* --- reciprocal_mul_256: 256.0/w * val * 256.0 --- */
static int32_t c_recip256_double(int32_t w, int32_t val) {
    return (int32_t)lrint(256.0 / (double)w * (double)val * 256.0);
}
static int32_t c_recip256_longdouble(int32_t w, int32_t val) {
    return (int32_t)lrintl(256.0L / (long double)w * (long double)val * 256.0L);
}
static int32_t c_recip256_volatile_ld(int32_t w, int32_t val) {
    volatile long double vw = (long double)w;
    volatile long double invW = 256.0L / vw;
    volatile long double result = invW * (long double)val;
    volatile long double result256 = result * 256.0L;
    return (int32_t)lrintl(result256);
}
static int32_t c_recip256_staged_ld(int32_t w, int32_t val) {
    volatile long double vw = (long double)w;
    volatile long double invW = (long double)256.0f / vw;
    volatile long double vval = (long double)val;
    volatile long double product = vval * invW;
    volatile long double result = product * (long double)256.0f;
    return (int32_t)lrintl(result);
}

/* ====================================================================
 * Test functions
 * ==================================================================== */

#define N_STRESS 500

static void test_fild_fistp_round(void) {
    printf("\n=== fild_fistp_round (identity through FPU) ===\n");
    int32_t vals[] = {0, 1, -1, 42, -42, 1000000, -1000000, 0x7FFFFFFF, (int32_t)0x80000000};
    for (int i = 0; i < (int)(sizeof(vals)/sizeof(vals[0])); i++) {
        int32_t asm_r = asm_fpu_fild_fistp_round(vals[i]);
        ASSERT_EQ_INT(vals[i], asm_r); /* identity check */
    }
    printf("  fild/fistp round-trip: all values preserved (as expected)\n");
}

static void test_int_div_int(void) {
    printf("\n=== int_div_int (round-to-nearest) ===\n");
    printf("  Comparing: double+lrint, long double+lrintl, volatile long double+lrintl\n");
    rng_seed(0xDEADBEEF);
    int match_d = 0, match_ld = 0, match_vld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        int32_t a = rng_range(-2000000, 2000000);
        int32_t b = rng_range(1, 100000);
        if (rng_next() & 1) b = -b;
        int32_t asm_r = asm_fpu_int_div_int(a, b);
        int32_t c_d  = c_int_div_int_double(a, b);
        int32_t c_ld = c_int_div_int_longdouble(a, b);
        int32_t c_vld = c_int_div_int_volatile_ld(a, b);
        char desc[128];
        snprintf(desc, sizeof(desc), "%d/%d", a, b);
        report_int_comparison("int_div_round", "double+lrint", asm_r, c_d, desc);
        report_int_comparison("int_div_round", "long_double+lrintl", asm_r, c_ld, desc);
        report_int_comparison("int_div_round", "volatile_ld+lrintl", asm_r, c_vld, desc);
        if (asm_r == c_d) match_d++;
        if (asm_r == c_ld) match_ld++;
        if (asm_r == c_vld) match_vld++;
    }
    printf("  Results (%d rounds): double=%d/%d  long_double=%d/%d  volatile_ld=%d/%d\n",
           N_STRESS, match_d, N_STRESS, match_ld, N_STRESS, match_vld, N_STRESS);
    ASSERT_TRUE(1);
}

static void test_int_div_int_trunc(void) {
    printf("\n=== int_div_int_trunc (truncation mode) ===\n");
    printf("  Comparing: (int32_t)(double), (int32_t)(long double), volatile_ld cast\n");
    rng_seed(0xCAFEBABE);
    int match_d = 0, match_ld = 0, match_vld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        int32_t a = rng_range(-2000000, 2000000);
        int32_t b = rng_range(1, 100000);
        if (rng_next() & 1) b = -b;
        int32_t asm_r = asm_fpu_int_div_int_trunc(a, b);
        int32_t c_d   = c_int_div_int_trunc_cast(a, b);
        int32_t c_ld  = c_int_div_int_trunc_ld(a, b);
        int32_t c_vld = c_int_div_int_trunc_volatile_ld(a, b);
        char desc[128];
        snprintf(desc, sizeof(desc), "%d/%d", a, b);
        report_int_comparison("int_div_trunc", "(int)(double)", asm_r, c_d, desc);
        report_int_comparison("int_div_trunc", "(int)(long_double)", asm_r, c_ld, desc);
        report_int_comparison("int_div_trunc", "volatile_ld_cast", asm_r, c_vld, desc);
        if (asm_r == c_d) match_d++;
        if (asm_r == c_ld) match_ld++;
        if (asm_r == c_vld) match_vld++;
    }
    printf("  Results (%d rounds): (int)(double)=%d/%d  (int)(long_double)=%d/%d  volatile_ld=%d/%d\n",
           N_STRESS, match_d, N_STRESS, match_ld, N_STRESS, match_vld, N_STRESS);
    ASSERT_TRUE(1);
}

static void test_reciprocal_mul(void) {
    printf("\n=== reciprocal_mul: 256.0/w * val (round-to-nearest) ===\n");
    printf("  Comparing: float, double, long_double, volatile_ld, staged_ld\n");
    rng_seed(0x12345678);
    int match_f = 0, match_d = 0, match_ld = 0, match_vld = 0, match_sld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        int32_t w = rng_range(1, 500000);
        int32_t val = rng_range(-500000, 500000);
        int32_t asm_r = asm_fpu_reciprocal_mul(w, val);
        int32_t c_f   = c_recip_mul_float(w, val);
        int32_t c_d   = c_recip_mul_double(w, val);
        int32_t c_ld  = c_recip_mul_longdouble(w, val);
        int32_t c_vld = c_recip_mul_volatile_ld(w, val);
        int32_t c_sld = c_recip_mul_staged_ld(w, val);
        char desc[128];
        snprintf(desc, sizeof(desc), "256/%d*%d", w, val);
        report_int_comparison("recip_mul", "float+lrintf", asm_r, c_f, desc);
        report_int_comparison("recip_mul", "double+lrint", asm_r, c_d, desc);
        report_int_comparison("recip_mul", "long_double+lrintl", asm_r, c_ld, desc);
        report_int_comparison("recip_mul", "volatile_ld+lrintl", asm_r, c_vld, desc);
        report_int_comparison("recip_mul", "staged_ld+lrintl", asm_r, c_sld, desc);
        if (asm_r == c_f) match_f++;
        if (asm_r == c_d) match_d++;
        if (asm_r == c_ld) match_ld++;
        if (asm_r == c_vld) match_vld++;
        if (asm_r == c_sld) match_sld++;
    }
    printf("  Results (%d rounds): float=%d  double=%d  long_double=%d  volatile_ld=%d  staged_ld=%d\n",
           N_STRESS, match_f, match_d, match_ld, match_vld, match_sld);
    ASSERT_TRUE(1);
}

static void test_slope(void) {
    printf("\n=== slope: dx/dy + 1.0 (round-to-nearest) ===\n");
    printf("  Comparing: double, long_double, volatile_ld, staged_ld\n");
    rng_seed(0xABCD1234);
    int match_d = 0, match_ld = 0, match_vld = 0, match_sld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        int32_t dx = rng_range(-500000, 500000);
        int32_t dy = rng_range(1, 100000);
        if (rng_next() & 1) dy = -dy;
        int32_t asm_r = asm_fpu_slope(dx, dy);
        int32_t c_d   = c_slope_double(dx, dy);
        int32_t c_ld  = c_slope_longdouble(dx, dy);
        int32_t c_vld = c_slope_volatile_ld(dx, dy);
        int32_t c_sld = c_slope_staged_ld(dx, dy);
        char desc[128];
        snprintf(desc, sizeof(desc), "%d/%d+1", dx, dy);
        report_int_comparison("slope_round", "double+lrint", asm_r, c_d, desc);
        report_int_comparison("slope_round", "long_double+lrintl", asm_r, c_ld, desc);
        report_int_comparison("slope_round", "volatile_ld+lrintl", asm_r, c_vld, desc);
        report_int_comparison("slope_round", "staged_ld+lrintl", asm_r, c_sld, desc);
        if (asm_r == c_d) match_d++;
        if (asm_r == c_ld) match_ld++;
        if (asm_r == c_vld) match_vld++;
        if (asm_r == c_sld) match_sld++;
    }
    printf("  Results (%d rounds): double=%d  long_double=%d  volatile_ld=%d  staged_ld=%d\n",
           N_STRESS, match_d, match_ld, match_vld, match_sld);
    ASSERT_TRUE(1);
}

static void test_slope_trunc(void) {
    printf("\n=== slope_trunc: dx/dy + 1.0 (truncation) ===\n");
    printf("  Comparing: double, long_double, volatile_ld, staged_ld\n");
    rng_seed(0xBEEFCAFE);
    int match_d = 0, match_ld = 0, match_vld = 0, match_sld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        int32_t dx = rng_range(-500000, 500000);
        int32_t dy = rng_range(1, 100000);
        if (rng_next() & 1) dy = -dy;
        int32_t asm_r = asm_fpu_slope_trunc(dx, dy);
        int32_t c_d   = c_slope_trunc_double(dx, dy);
        int32_t c_ld  = c_slope_trunc_longdouble(dx, dy);
        int32_t c_vld = c_slope_trunc_volatile_ld(dx, dy);
        int32_t c_sld = c_slope_trunc_staged_ld(dx, dy);
        char desc[128];
        snprintf(desc, sizeof(desc), "%d/%d+1", dx, dy);
        report_int_comparison("slope_trunc", "(int)(double)", asm_r, c_d, desc);
        report_int_comparison("slope_trunc", "(int)(long_double)", asm_r, c_ld, desc);
        report_int_comparison("slope_trunc", "volatile_ld_cast", asm_r, c_vld, desc);
        report_int_comparison("slope_trunc", "staged_ld_cast", asm_r, c_sld, desc);
        if (asm_r == c_d) match_d++;
        if (asm_r == c_ld) match_ld++;
        if (asm_r == c_vld) match_vld++;
        if (asm_r == c_sld) match_sld++;
    }
    printf("  Results (%d rounds): double=%d  long_double=%d  volatile_ld=%d  staged_ld=%d\n",
           N_STRESS, match_d, match_ld, match_vld, match_sld);
    ASSERT_TRUE(1);
}

static void test_interp_factor(void) {
    printf("\n=== interp_factor: Z-clip interpolation ===\n");
    printf("  Comparing: double, long_double, volatile_ld\n");
    rng_seed(0xFEEDFACE);
    int match_d = 0, match_ld = 0, match_vld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        int32_t lastZ = rng_range(-500, 500);
        int32_t curZ  = rng_range(-500, 500);
        /* Ensure curZ != lastZ to avoid division by zero */
        if (curZ == lastZ) curZ = lastZ + 1;
        int32_t zclip = rng_range(-500, 500);
        int32_t lastVal = rng_range(-30000, 30000);
        int32_t curVal  = rng_range(-30000, 30000);

        int32_t asm_r = asm_fpu_interp_factor(zclip, lastZ, curZ, lastVal, curVal);
        int32_t c_d   = c_interp_double(zclip, lastZ, curZ, lastVal, curVal);
        int32_t c_ld  = c_interp_longdouble(zclip, lastZ, curZ, lastVal, curVal);
        int32_t c_vld = c_interp_volatile_ld(zclip, lastZ, curZ, lastVal, curVal);
        char desc[128];
        snprintf(desc, sizeof(desc), "zc=%d lz=%d cz=%d lv=%d cv=%d",
                 zclip, lastZ, curZ, lastVal, curVal);
        report_int_comparison("interp", "double+lrint", asm_r, c_d, desc);
        report_int_comparison("interp", "long_double+lrintl", asm_r, c_ld, desc);
        report_int_comparison("interp", "volatile_ld+lrintl", asm_r, c_vld, desc);
        if (asm_r == c_d) match_d++;
        if (asm_r == c_ld) match_ld++;
        if (asm_r == c_vld) match_vld++;
    }
    printf("  Results (%d rounds): double=%d  long_double=%d  volatile_ld=%d\n",
           N_STRESS, match_d, match_ld, match_vld);
    ASSERT_TRUE(1);
}

static void test_float_to_int_round(void) {
    printf("\n=== float_to_int (round-to-nearest) ===\n");
    printf("  Comparing: lrintf, lrintl(long double)\n");
    rng_seed(0x11223344);
    int match_f = 0, match_ld = 0;
    float test_vals[] = {
        0.5f, 1.5f, 2.5f, 3.5f, -0.5f, -1.5f, -2.5f, -3.5f,
        0.4999f, 0.5001f, 1.4999f, 1.5001f,
        100.7f, -100.7f, 0.0f, 1.0f, -1.0f,
        123456.789f, -123456.789f
    };
    for (int i = 0; i < (int)(sizeof(test_vals)/sizeof(test_vals[0])); i++) {
        int32_t asm_r = asm_fpu_float_to_int_round(test_vals[i]);
        int32_t c_f   = c_ftoi_round_lrint(test_vals[i]);
        int32_t c_ld  = c_ftoi_round_lrintl_ld(test_vals[i]);
        char desc[64];
        snprintf(desc, sizeof(desc), "%.6f", test_vals[i]);
        report_int_comparison("ftoi_round", "lrintf", asm_r, c_f, desc);
        report_int_comparison("ftoi_round", "lrintl(ld)", asm_r, c_ld, desc);
        if (asm_r == c_f) match_f++;
        if (asm_r == c_ld) match_ld++;
    }
    int n = (int)(sizeof(test_vals)/sizeof(test_vals[0]));
    printf("  Results (%d cases): lrintf=%d/%d  lrintl(ld)=%d/%d\n",
           n, match_f, n, match_ld, n);
    ASSERT_TRUE(1);
}

static void test_float_to_int_trunc(void) {
    printf("\n=== float_to_int (truncation) ===\n");
    float test_vals[] = {
        0.9f, -0.9f, 1.9f, -1.9f, 100.99f, -100.99f,
        0.0f, 1.0f, -1.0f, 0.5f, -0.5f
    };
    int match = 0;
    for (int i = 0; i < (int)(sizeof(test_vals)/sizeof(test_vals[0])); i++) {
        int32_t asm_r = asm_fpu_float_to_int_trunc(test_vals[i]);
        int32_t c_r   = c_ftoi_trunc_cast(test_vals[i]);
        char desc[64];
        snprintf(desc, sizeof(desc), "%.6f", test_vals[i]);
        report_int_comparison("ftoi_trunc", "(int32_t)cast", asm_r, c_r, desc);
        if (asm_r == c_r) match++;
    }
    int n = (int)(sizeof(test_vals)/sizeof(test_vals[0]));
    printf("  Results (%d cases): (int32_t)cast=%d/%d\n", n, match, n);
    ASSERT_TRUE(1);
}

static void test_mul_add_chain(void) {
    printf("\n=== mul_add_chain: a*b + c*d (float result) ===\n");
    printf("  Comparing: float, double, long_double, volatile_ld\n");
    rng_seed(0x55667788);
    int match_f = 0, match_d = 0, match_ld = 0, match_vld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        float a = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        float b = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        float c = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        float d = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        float asm_r;
        asm_fpu_mul_add_chain(a, b, c, d, &asm_r);
        float c_f   = c_mul_add_float(a, b, c, d);
        float c_d   = c_mul_add_double(a, b, c, d);
        float c_ld  = c_mul_add_longdouble(a, b, c, d);
        float c_vld = c_mul_add_volatile_ld(a, b, c, d);
        char desc[128];
        snprintf(desc, sizeof(desc), "%.3f*%.3f+%.3f*%.3f", a, b, c, d);
        report_float_comparison("mul_add", "float_native", asm_r, c_f, desc);
        report_float_comparison("mul_add", "via_double", asm_r, c_d, desc);
        report_float_comparison("mul_add", "via_long_double", asm_r, c_ld, desc);
        report_float_comparison("mul_add", "volatile_ld", asm_r, c_vld, desc);
        uint32_t ab, af, ad, ald, avld;
        memcpy(&ab, &asm_r, 4); memcpy(&af, &c_f, 4);
        memcpy(&ad, &c_d, 4); memcpy(&ald, &c_ld, 4); memcpy(&avld, &c_vld, 4);
        if (ab == af) match_f++;
        if (ab == ad) match_d++;
        if (ab == ald) match_ld++;
        if (ab == avld) match_vld++;
    }
    printf("  Results (%d rounds): float=%d  double=%d  long_double=%d  volatile_ld=%d\n",
           N_STRESS, match_f, match_d, match_ld, match_vld);
    ASSERT_TRUE(1);
}

static void test_mul_sub_chain(void) {
    printf("\n=== mul_sub_chain: a*b - c*d (float result) ===\n");
    printf("  Comparing: float, double, long_double\n");
    rng_seed(0x99AABBCC);
    int match_f = 0, match_d = 0, match_ld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        float a = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        float b = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        float c = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        float d = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        float asm_r;
        asm_fpu_mul_sub_chain(a, b, c, d, &asm_r);
        float c_f  = c_mul_sub_float(a, b, c, d);
        float c_d  = c_mul_sub_double(a, b, c, d);
        float c_ld = c_mul_sub_longdouble(a, b, c, d);
        char desc[128];
        snprintf(desc, sizeof(desc), "%.3f*%.3f-%.3f*%.3f", a, b, c, d);
        report_float_comparison("mul_sub", "float_native", asm_r, c_f, desc);
        report_float_comparison("mul_sub", "via_double", asm_r, c_d, desc);
        report_float_comparison("mul_sub", "via_long_double", asm_r, c_ld, desc);
        uint32_t ab, af, ad, ald;
        memcpy(&ab, &asm_r, 4); memcpy(&af, &c_f, 4);
        memcpy(&ad, &c_d, 4); memcpy(&ald, &c_ld, 4);
        if (ab == af) match_f++;
        if (ab == ad) match_d++;
        if (ab == ald) match_ld++;
    }
    printf("  Results (%d rounds): float=%d  double=%d  long_double=%d\n",
           N_STRESS, match_f, match_d, match_ld);
    ASSERT_TRUE(1);
}

static void test_dot3(void) {
    printf("\n=== dot3: float dot product ===\n");
    printf("  Comparing: float, double, long_double\n");
    rng_seed(0xDDEEFF00);
    int match_f = 0, match_d = 0, match_ld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        float a[3], b[3];
        for (int j = 0; j < 3; j++) {
            a[j] = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
            b[j] = (float)rng_range(-1000, 1000) + (float)(rng_next() % 1000) / 1000.0f;
        }
        float asm_r;
        asm_fpu_dot3(a, b, &asm_r);
        float c_f  = c_dot3_float(a, b);
        float c_d  = c_dot3_double(a, b);
        float c_ld = c_dot3_longdouble(a, b);
        char desc[128];
        snprintf(desc, sizeof(desc), "[%.1f,%.1f,%.1f].[%.1f,%.1f,%.1f]",
                 a[0], a[1], a[2], b[0], b[1], b[2]);
        report_float_comparison("dot3", "float_native", asm_r, c_f, desc);
        report_float_comparison("dot3", "via_double", asm_r, c_d, desc);
        report_float_comparison("dot3", "via_long_double", asm_r, c_ld, desc);
        uint32_t ab_bits, af, ad, ald;
        memcpy(&ab_bits, &asm_r, 4); memcpy(&af, &c_f, 4);
        memcpy(&ad, &c_d, 4); memcpy(&ald, &c_ld, 4);
        if (ab_bits == af) match_f++;
        if (ab_bits == ad) match_d++;
        if (ab_bits == ald) match_ld++;
    }
    printf("  Results (%d rounds): float=%d  double=%d  long_double=%d\n",
           N_STRESS, match_f, match_d, match_ld);
    ASSERT_TRUE(1);
}

static void test_perspective_uv(void) {
    printf("\n=== perspective_uv: 256/w * u, 256/w * v (cached invW) ===\n");
    printf("  Comparing: volatile_ld+lrintl, staged_ld+lrintl, double+lrint\n");
    rng_seed(0xAAAABBBB);
    int match_vld = 0, match_sld = 0, match_d = 0;
    for (int i = 0; i < N_STRESS; i++) {
        int32_t w = rng_range(1, 500000);
        int32_t u = rng_range(-500000, 500000);
        int32_t v = rng_range(-500000, 500000);
        int32_t asm_ou, asm_ov;
        asm_fpu_perspective_uv(w, u, v, &asm_ou, &asm_ov);

        /* double approach */
        double dinvW = 256.0 / (double)w;
        int32_t d_u = (int32_t)lrint(dinvW * (double)u);
        int32_t d_v = (int32_t)lrint(dinvW * (double)v);

        /* volatile long double approach */
        volatile long double vw = (long double)w;
        volatile long double invW = 256.0L / vw;
        volatile long double ru = invW * (long double)u;
        volatile long double rv = invW * (long double)v;
        int32_t vld_u = (int32_t)lrintl(ru);
        int32_t vld_v = (int32_t)lrintl(rv);

        /* staged long double with float constant */
        volatile long double sw = (long double)w;
        volatile long double sinvW = (long double)256.0f / sw;
        volatile long double sru = (long double)u * sinvW;
        volatile long double srv = (long double)v * sinvW;
        int32_t sld_u = (int32_t)lrintl(sru);
        int32_t sld_v = (int32_t)lrintl(srv);

        char desc[128];
        snprintf(desc, sizeof(desc), "256/%d*(%d,%d)", w, u, v);
        report_int_comparison("persp_uv_u", "double+lrint", asm_ou, d_u, desc);
        report_int_comparison("persp_uv_v", "double+lrint", asm_ov, d_v, desc);
        report_int_comparison("persp_uv_u", "volatile_ld", asm_ou, vld_u, desc);
        report_int_comparison("persp_uv_v", "volatile_ld", asm_ov, vld_v, desc);
        report_int_comparison("persp_uv_u", "staged_ld", asm_ou, sld_u, desc);
        report_int_comparison("persp_uv_v", "staged_ld", asm_ov, sld_v, desc);
        if (asm_ou == d_u && asm_ov == d_v) match_d++;
        if (asm_ou == vld_u && asm_ov == vld_v) match_vld++;
        if (asm_ou == sld_u && asm_ov == sld_v) match_sld++;
    }
    printf("  Results (%d rounds): double=%d  volatile_ld=%d  staged_ld=%d\n",
           N_STRESS, match_d, match_vld, match_sld);
    ASSERT_TRUE(1);
}

static void test_reciprocal_mul_256(void) {
    printf("\n=== reciprocal_mul_256: 256/w * val * 256 ===\n");
    printf("  Comparing: double, long_double, volatile_ld, staged_ld\n");
    rng_seed(0xCCDDEEFF);
    int match_d = 0, match_ld = 0, match_vld = 0, match_sld = 0;
    for (int i = 0; i < N_STRESS; i++) {
        int32_t w = rng_range(1, 500000);
        int32_t val = rng_range(-500000, 500000);
        int32_t asm_r = asm_fpu_reciprocal_mul_256(w, val);
        int32_t c_d   = c_recip256_double(w, val);
        int32_t c_ld  = c_recip256_longdouble(w, val);
        int32_t c_vld = c_recip256_volatile_ld(w, val);
        int32_t c_sld = c_recip256_staged_ld(w, val);
        char desc[128];
        snprintf(desc, sizeof(desc), "256/%d*%d*256", w, val);
        report_int_comparison("recip256", "double+lrint", asm_r, c_d, desc);
        report_int_comparison("recip256", "long_double+lrintl", asm_r, c_ld, desc);
        report_int_comparison("recip256", "volatile_ld+lrintl", asm_r, c_vld, desc);
        report_int_comparison("recip256", "staged_ld+lrintl", asm_r, c_sld, desc);
        if (asm_r == c_d) match_d++;
        if (asm_r == c_ld) match_ld++;
        if (asm_r == c_vld) match_vld++;
        if (asm_r == c_sld) match_sld++;
    }
    printf("  Results (%d rounds): double=%d  long_double=%d  volatile_ld=%d  staged_ld=%d\n",
           N_STRESS, match_d, match_ld, match_vld, match_sld);
    ASSERT_TRUE(1);
}

/* ====================================================================
 * Main
 * ==================================================================== */
int main(void) {
    printf("FPU Precision Test Suite — x87 ASM vs Portable C Approaches\n");
    printf("=============================================================\n");
    printf("Each test runs %d random rounds and reports match rate per C approach.\n", N_STRESS);
    printf("Mismatches are printed individually for debugging.\n");

    RUN_TEST(test_fild_fistp_round);
    RUN_TEST(test_int_div_int);
    RUN_TEST(test_int_div_int_trunc);
    RUN_TEST(test_reciprocal_mul);
    RUN_TEST(test_slope);
    RUN_TEST(test_slope_trunc);
    RUN_TEST(test_interp_factor);
    RUN_TEST(test_float_to_int_round);
    RUN_TEST(test_float_to_int_trunc);
    RUN_TEST(test_mul_add_chain);
    RUN_TEST(test_mul_sub_chain);
    RUN_TEST(test_dot3);
    RUN_TEST(test_perspective_uv);
    RUN_TEST(test_reciprocal_mul_256);

    printf("\n=============================================================\n");
    printf("OVERALL PRECISION SUMMARY\n");
    printf("  Total comparisons: %d\n", total_comparisons);
    printf("  Exact matches:     %d (%.1f%%)\n",
           total_matches, total_comparisons ? 100.0 * total_matches / total_comparisons : 0.0);
    printf("  Mismatches:        %d (%.1f%%)\n",
           total_mismatches, total_comparisons ? 100.0 * total_mismatches / total_comparisons : 0.0);
    printf("=============================================================\n");

    TEST_SUMMARY();
    return test_failures != 0;
}
