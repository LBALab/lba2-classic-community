/* Host test: which display states may use the double-wide stereo target
 * (LIB386/H/GL/STEREO_POLICY.H).
 *
 * The isometric interior only gets parallax on the double-wide single-pass
 * path: its projection is affine and ignores the per-eye camera shift, so the
 * per-eye fallback renders both halves from one camera.  That makes this
 * predicate load-bearing for interior depth, and it must not quietly exclude
 * AA off — the shipping default — the way it used to.
 *
 * CPP-only (no ASM, no Docker, no game assets). */
#include "test_harness.h"

#include <GL/STEREO_POLICY.H>

void test_allowed_with_aa_off(void) {
    /* AA off is the default and renders the pair directly. */
    ASSERT_TRUE(GL_StereoDoubleWideAllowedFor(1, 1, 1, 0) != 0);
}

void test_allowed_with_msaa(void) {
    ASSERT_TRUE(GL_StereoDoubleWideAllowedFor(1, 1, 1, 2) != 0);
    ASSERT_TRUE(GL_StereoDoubleWideAllowedFor(1, 1, 1, 3) != 0);
}

void test_fxaa_keeps_the_per_eye_path(void) {
    /* FXAA is a per-eye composite post pass with no double-wide equivalent. */
    ASSERT_TRUE(GL_StereoDoubleWideAllowedFor(1, 1, 1, 1) == 0);
}

void test_requires_stereo_state(void) {
    ASSERT_TRUE(GL_StereoDoubleWideAllowedFor(0, 1, 1, 0) == 0);
    ASSERT_TRUE(GL_StereoDoubleWideAllowedFor(1, 0, 1, 0) == 0);
    ASSERT_TRUE(GL_StereoDoubleWideAllowedFor(1, 1, 0, 0) == 0);
}

int main(void) {
    RUN_TEST(test_allowed_with_aa_off);
    RUN_TEST(test_allowed_with_msaa);
    RUN_TEST(test_fxaa_keeps_the_per_eye_path);
    RUN_TEST(test_requires_stereo_state);
    TEST_SUMMARY();
    return test_failures != 0;
}
