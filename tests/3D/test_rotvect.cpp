/* Test: RotateVector — rotate vector by Euler angles; writes X0,Y0,Z0 */
#include "test_harness.h"

#include <3D/ROTVECT.H>
#include <3D/CAMERA.H>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_RotateVector(S32 norme, S32 alpha, S32 beta, S32 gamma);
#endif

static void test_no_rotation(void)
{
    RotateVector(100, 0, 0, 0);
    /* norme along Z with no rotation → X0=0, Y0=0, Z0=100 */
    ASSERT_EQ_INT(0, X0);
    ASSERT_EQ_INT(0, Y0);
    ASSERT_EQ_INT(100, Z0);
}

static void test_zero_norme(void)
{
    RotateVector(0, 1024, 512, 2048);
    ASSERT_EQ_INT(0, X0);
    ASSERT_EQ_INT(0, Y0);
    ASSERT_EQ_INT(0, Z0);
}

static void test_90deg_beta(void)
{
    /* 90° around Y: Z→X */
    RotateVector(100, 0, 1024, 0);
    ASSERT_TRUE(X0 >= 95 && X0 <= 105);
    ASSERT_TRUE(Y0 >= -5 && Y0 <= 5);
    ASSERT_TRUE(Z0 >= -5 && Z0 <= 5);
}

static void test_negative_norme(void)
{
    RotateVector(-100, 0, 0, 0);
    ASSERT_EQ_INT(0, X0);
    ASSERT_EQ_INT(0, Y0);
    ASSERT_EQ_INT(-100, Z0);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    struct { S32 n, a, b, g; } cases[] = {
        {0, 0, 0, 0}, {100, 0, 0, 0}, {100, 1024, 0, 0},
        {100, 0, 1024, 0}, {100, 0, 0, 1024},
        {500, 512, 512, 512}, {-100, 2048, 1024, 3072},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        RotateVector(cases[i].n, cases[i].a, cases[i].b, cases[i].g);
        S32 cpp_x = X0, cpp_y = Y0, cpp_z = Z0;
        asm_RotateVector(cases[i].n, cases[i].a, cases[i].b, cases[i].g);
        ASSERT_ASM_CPP_EQ_INT(X0, cpp_x, "RotateVector X0");
        ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y, "RotateVector Y0");
        ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z, "RotateVector Z0");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_no_rotation);
    RUN_TEST(test_zero_norme);
    RUN_TEST(test_90deg_beta);
    RUN_TEST(test_negative_norme);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
