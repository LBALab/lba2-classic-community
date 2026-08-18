/*
 * Drives the long double / lrintl rotation paths so the sanitizers can watch
 * them.
 *
 * Issue #138 asked whether the x87 precision code conflicts with UBSan and left
 * it as an open sanity check, because the only tests that touch these functions
 * are ASM-equivalence tests (tests/3D, tests/OBJECT, tests/ANIM). Those need the
 * 32-bit UASM toolchain and run in the Docker leg, so the `sanitize` job in
 * linux.yml, which builds `host_tests` and runs `ctest -L host_quick`, never
 * reaches this code at all.
 *
 * What this is for: coverage under the sanitizers, not correctness. The
 * ASM-equivalence tests already own correctness, and they compare against the
 * original to a far higher standard than anything asserted here. This drives the
 * same entry points over a spread of inputs, including the ones most likely to
 * upset a conversion (the angle wrap, the quadrant boundaries, and coordinates
 * near the S32 edges where `long double` -> `lrintl` has to land somewhere
 * defined). Under `linux_sanitize` a signed overflow, a bad shift or an invalid
 * float-to-int conversion in that path fails the run.
 *
 * The assertions are deliberately weak but true: the same input gives the same
 * output, and the identity angle leaves a point alone. Anything stronger would
 * either restate the ASM comparison or invent a precision contract this file has
 * no standing to define.
 */

#include "test_harness.h"

#include <3D/CAMERA.H>
#include <3D/ROT2D.H>

#include <stdlib.h>

/* LongRotate writes its result to these. CAMERA.CPP defines them for the engine
   but brings the whole matrix and projection stack with it, so the test owns
   them here instead. */
S32 X0 = 0;
S32 Y0 = 0;
S32 Z0 = 0;

/* Angles are 12-bit (0..4095). These are the wrap and quadrant boundaries plus a
   few interior values; `LongRotate` special-cases angle 0. */
static const S32 kAngles[] = {0, 1, 1023, 1024, 1025, 2047, 2048,
                              2049, 3071, 3072, 3073, 4094, 4095, 4096, 8191};

/* Coordinates spanning small, scene-sized and extreme magnitudes. The large ones
   matter: the products feed a long double before `lrintl`, and that is where a
   conversion would go undefined if the arithmetic were done narrower. */
static const S32 kCoords[] = {0, 1, -1, 512, -512,
                              32767, -32768, 1000000, -1000000, 16777216,
                              -16777216};

static void test_rotate_is_deterministic(void) {
    for (unsigned a = 0; a < sizeof(kAngles) / sizeof(kAngles[0]); a++) {
        for (unsigned i = 0; i < sizeof(kCoords) / sizeof(kCoords[0]); i++) {
            for (unsigned j = 0; j < sizeof(kCoords) / sizeof(kCoords[0]); j++) {
                LongRotate(kCoords[i], kCoords[j], kAngles[a]);
                const S32 x1 = X0;
                const S32 z1 = Z0;

                LongRotate(kCoords[i], kCoords[j], kAngles[a]);

                ASSERT_EQ_INT(x1, X0);
                ASSERT_EQ_INT(z1, Z0);
            }
        }
    }
}

static void test_identity_angle_is_a_no_op(void) {
    for (unsigned i = 0; i < sizeof(kCoords) / sizeof(kCoords[0]); i++) {
        for (unsigned j = 0; j < sizeof(kCoords) / sizeof(kCoords[0]); j++) {
            LongRotate(kCoords[i], kCoords[j], 0);

            ASSERT_EQ_INT(kCoords[i], X0);
            ASSERT_EQ_INT(kCoords[j], Z0);
        }
    }
}

/* Rotate() is the shipping entry point and forwards to LongRotate(); drive it so
   the wrapper is covered too rather than only its target. */
static void test_rotate_wrapper_matches(void) {
    for (unsigned a = 0; a < sizeof(kAngles) / sizeof(kAngles[0]); a++) {
        LongRotate(12345, -6789, kAngles[a]);
        const S32 x1 = X0;
        const S32 z1 = Z0;

        Rotate(12345, -6789, kAngles[a]);

        ASSERT_EQ_INT(x1, X0);
        ASSERT_EQ_INT(z1, Z0);
    }
}

int main(void) {
    RUN_TEST(test_rotate_is_deterministic);
    RUN_TEST(test_identity_angle_is_a_no_op);
    RUN_TEST(test_rotate_wrapper_matches);
    TEST_SUMMARY();
    return test_failures != 0;
}
