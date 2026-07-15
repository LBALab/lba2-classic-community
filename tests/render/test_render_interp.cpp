/* Render-interpolation core (issue #412, docs/RENDER_INTERP_PLAN.md Phase 1).
 *
 * Render_LerpCoord draws a world coordinate between the previous and current sim-step positions by a
 * banked-time fraction, so held movement is smooth on a display faster than the ~60 Hz sim. The two
 * properties that matter: the endpoints are byte-exact (a frame on a step boundary matches the
 * un-interpolated position, so the projection golden is untouched), and large world spans do not
 * overflow the fixed-point math. Render_InterpNum derives the fraction numerator from the game clock
 * and clamps the off / boundary / backwards-clock cases so the lerp never divides by zero or reads a
 * huge unsigned value.
 *
 * CPP-only host test, no game data. Compiles the real LIB386/3D/RENDERINTERP.CPP.
 */
#include "../test_harness.h"
#include <3D/RENDERINTERP.H>

/* num=0 -> prev exactly, num=den -> cur exactly, both signs. This is the byte-exact guarantee the
   projection golden depends on. */
static void test_lerp_endpoints_byte_exact(void) {
    ASSERT_EQ_INT(1000, Render_LerpCoord(1000, 2000, 0, 16));
    ASSERT_EQ_INT(2000, Render_LerpCoord(1000, 2000, 16, 16));
    ASSERT_EQ_INT(2000, Render_LerpCoord(2000, 1000, 0, 16)); /* negative delta */
    ASSERT_EQ_INT(1000, Render_LerpCoord(2000, 1000, 16, 16));
}

/* Interior fractions, both signs. */
static void test_lerp_midpoint_and_fractions(void) {
    ASSERT_EQ_INT(1500, Render_LerpCoord(1000, 2000, 8, 16)); /* half   */
    ASSERT_EQ_INT(1250, Render_LerpCoord(1000, 2000, 4, 16)); /* quarter */
    ASSERT_EQ_INT(1500, Render_LerpCoord(2000, 1000, 8, 16)); /* half, negative delta */
    ASSERT_EQ_INT(10250, Render_LerpCoord(10000, 11000, 4, 16));
}

/* Big coords + span: the 64-bit intermediate must not overflow a 32-bit product. */
static void test_lerp_no_overflow_large_span(void) {
    ASSERT_EQ_INT(0, Render_LerpCoord(-30000, 30000, 8, 16));      /* midpoint of a 60000 span */
    ASSERT_EQ_INT(26250, Render_LerpCoord(-30000, 30000, 15, 16)); /* 15/16 of 60000 = 56250 */
}

/* den==0 (interpolation off / no throttle) and num>=den both return cur, never divide by zero. */
static void test_lerp_off_and_bounds(void) {
    ASSERT_EQ_INT(2000, Render_LerpCoord(1000, 2000, 0, 0));
    ASSERT_EQ_INT(2000, Render_LerpCoord(1000, 2000, 5, 0));
    ASSERT_EQ_INT(2000, Render_LerpCoord(1000, 2000, 99, 16)); /* num past den -> cur */
}

/* Banked fraction numerator sweeps [0, dt] and clamps at the boundary. */
static void test_interp_num_sweep(void) {
    ASSERT_EQ_UINT(0u, Render_InterpNum(1000, 1000, 16));  /* just stepped */
    ASSERT_EQ_UINT(4u, Render_InterpNum(1004, 1000, 16));  /* 4 ms into a 16 ms step */
    ASSERT_EQ_UINT(16u, Render_InterpNum(1016, 1000, 16)); /* at boundary -> dt (renders cur) */
    ASSERT_EQ_UINT(16u, Render_InterpNum(1020, 1000, 16)); /* past boundary -> clamped to dt */
}

/* dt<=0 disables; a backwards clock must not underflow to a huge value. */
static void test_interp_num_off_and_backwards(void) {
    ASSERT_EQ_UINT(0u, Render_InterpNum(1004, 1000, 0));
    ASSERT_EQ_UINT(0u, Render_InterpNum(1000, 1004, 16));
}

int main(void) {
    RUN_TEST(test_lerp_endpoints_byte_exact);
    RUN_TEST(test_lerp_midpoint_and_fractions);
    RUN_TEST(test_lerp_no_overflow_large_span);
    RUN_TEST(test_lerp_off_and_bounds);
    RUN_TEST(test_interp_num_sweep);
    RUN_TEST(test_interp_num_off_and_backwards);
    TEST_SUMMARY();
    return test_failures != 0;
}
