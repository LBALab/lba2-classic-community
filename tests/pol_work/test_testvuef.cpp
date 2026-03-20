/* Test: TestVuePoly — polygon visibility / backface culling.
 *
 * TestVuePoly checks polygon winding order via cross product of the
 * first three vertices.  Returns 1 if visible (CW), 0 if not (CCW).
 *
 * ASM note: TestVuePoly in ASM is a DATA variable (function pointer)
 * pointing to TestVuePolyF (the actual PROC).  TestVuePolyF uses
 * C-adapted calling convention (PROC uses ... input: DWORD).
 */
#include "test_harness.h"
#include <POLYGON/TESTVUE.H>
#include <POLYGON/POLY.H>
#include <string.h>

/* ASM-side: TestVuePolyF is the actual function.
 * With STRIP_C_ADAPT, it uses Watcom register convention: input in ESI.
 * TestVuePoly is a dd (function pointer) in ASM data. */
extern "C" S32 asm_TestVuePolyF(void);

static inline S32 call_asm_TestVuePolyF(Struc_Point *ptr) {
    S32 result;
    __asm__ __volatile__(
        "call asm_TestVuePolyF"
        : "=a"(result)
        : "S"(ptr)
        : "ebx", "ecx", "edx", "edi", "memory", "cc");
    return result;
}

static Struc_Point pts[4];

/* Helper: build triangle from 3 screen-coordinate pairs. */
static void set_tri(S16 x0, S16 y0, S16 x1, S16 y1, S16 x2, S16 y2) {
    memset(pts, 0, sizeof(pts));
    pts[0].Pt_XE = x0;
    pts[0].Pt_YE = y0;
    pts[1].Pt_XE = x1;
    pts[1].Pt_YE = y1;
    pts[2].Pt_XE = x2;
    pts[2].Pt_YE = y2;
}

/* ── CPP-only sanity checks ────────────────────────────────────── */

static void test_cw_visible(void) {
    /* In screen coords (Y down), CW winding = visible.
     * (0,0)->(50,80)->(100,0) is CW in screen coords. */
    set_tri(0, 0, 50, 80, 100, 0);
    S32 result = TestVuePoly(pts);
    ASSERT_EQ_INT(1, result);
}

static void test_ccw_invisible(void) {
    /* (0,0)->(100,0)->(50,80) is CCW in screen coords → invisible */
    set_tri(0, 0, 100, 0, 50, 80);
    S32 result = TestVuePoly(pts);
    ASSERT_EQ_INT(0, result);
}

static void test_collinear(void) {
    /* Collinear: cross product == 0, should return 1 (>= 0) */
    set_tri(0, 0, 50, 50, 100, 100);
    S32 result = TestVuePoly(pts);
    ASSERT_EQ_INT(1, result);
}

static void test_single_point(void) {
    /* Degenerate: all same point */
    set_tri(42, 42, 42, 42, 42, 42);
    S32 result = TestVuePoly(pts);
    ASSERT_EQ_INT(1, result);
}

static void test_large_coords(void) {
    /* Near S16 limits — CW in screen coords */
    set_tri(-30000, -30000, 0, 30000, 30000, -30000);
    S32 result = TestVuePoly(pts);
    ASSERT_EQ_INT(1, result);
}

/* ── ASM-vs-CPP equivalence ────────────────────────────────────── */

static void test_asm_equiv_cw(void) {
    set_tri(0, 0, 100, 0, 50, 80);
    S32 cpp = TestVuePoly(pts);
    S32 asmr = call_asm_TestVuePolyF(pts);
    ASSERT_ASM_CPP_EQ_INT(asmr, cpp, "TestVuePoly CW");
}

static void test_asm_equiv_ccw(void) {
    set_tri(0, 0, 50, 80, 100, 0);
    S32 cpp = TestVuePoly(pts);
    S32 asmr = call_asm_TestVuePolyF(pts);
    ASSERT_ASM_CPP_EQ_INT(asmr, cpp, "TestVuePoly CCW");
}

static void test_asm_equiv_collinear(void) {
    set_tri(0, 0, 50, 50, 100, 100);
    S32 cpp = TestVuePoly(pts);
    S32 asmr = call_asm_TestVuePolyF(pts);
    ASSERT_ASM_CPP_EQ_INT(asmr, cpp, "TestVuePoly collinear");
}

static void test_asm_equiv_large(void) {
    set_tri(-30000, -30000, 30000, -30000, 0, 30000);
    S32 cpp = TestVuePoly(pts);
    S32 asmr = call_asm_TestVuePolyF(pts);
    ASSERT_ASM_CPP_EQ_INT(asmr, cpp, "TestVuePoly large coords");
}

/* ── Randomized stress test ────────────────────────────────────── */

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

static void test_asm_equiv_random(void) {
    rng_seed(0xDEADBEEF);
    for (int i = 0; i < 300; i++) {
        S16 x0 = (S16)(rng_next() % 640) - 320;
        S16 y0 = (S16)(rng_next() % 480) - 240;
        S16 x1 = (S16)(rng_next() % 640) - 320;
        S16 y1 = (S16)(rng_next() % 480) - 240;
        S16 x2 = (S16)(rng_next() % 640) - 320;
        S16 y2 = (S16)(rng_next() % 480) - 240;
        set_tri(x0, y0, x1, y1, x2, y2);
        S32 cpp = TestVuePoly(pts);
        S32 asmr = call_asm_TestVuePolyF(pts);
        ASSERT_ASM_CPP_EQ_INT(asmr, cpp, "TestVuePoly random");
    }
}

int main(void) {
    RUN_TEST(test_cw_visible);
    RUN_TEST(test_ccw_invisible);
    RUN_TEST(test_collinear);
    RUN_TEST(test_single_point);
    RUN_TEST(test_large_coords);
    RUN_TEST(test_asm_equiv_cw);
    RUN_TEST(test_asm_equiv_ccw);
    RUN_TEST(test_asm_equiv_collinear);
    RUN_TEST(test_asm_equiv_large);
    RUN_TEST(test_asm_equiv_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
