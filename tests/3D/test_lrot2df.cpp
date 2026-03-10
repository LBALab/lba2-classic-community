/* Test: LongRotate / Rotate — 2D rotation by angle; writes X0,Z0 */
#include "test_harness.h"

#include <3D/ROT2D.H>
#include <3D/CAMERA.H>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_LongRotate(S32 x, S32 z, S32 angle);
#endif

static void test_zero_angle(void)
{
    LongRotate(100, 200, 0);
    ASSERT_EQ_INT(100, X0);
    ASSERT_EQ_INT(200, Z0);
}

static void test_90deg(void)
{
    /* 90° = 1024 */
    LongRotate(100, 0, 1024);
    /* cos(90)=0, sin(90)=1 → X0 = x*cos + z*sin = 0, Z0 = z*cos - x*sin = -100 */
    ASSERT_TRUE(X0 >= -5 && X0 <= 5);       /* ≈0 */
    ASSERT_TRUE(Z0 >= -105 && Z0 <= -95);   /* ≈-100 */
}

static void test_180deg(void)
{
    LongRotate(100, 200, 2048);
    ASSERT_TRUE(X0 >= -105 && X0 <= -95);
    ASSERT_TRUE(Z0 >= -205 && Z0 <= -195);
}

static void test_360deg(void)
{
    LongRotate(100, 200, 4096);
    /* Full rotation = no change (wraps via &4095 → 0) */
    ASSERT_EQ_INT(100, X0);
    ASSERT_EQ_INT(200, Z0);
}

static void test_negative_angle(void)
{
    LongRotate(100, 0, -1024);
    /* -90° → X0≈0, Z0≈100 */
    ASSERT_TRUE(X0 >= -5 && X0 <= 5);
    ASSERT_TRUE(Z0 >= 95 && Z0 <= 105);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    struct { S32 x, z, angle; } cases[] = {
        {0, 0, 0}, {100, 0, 0}, {100, 200, 0},
        {100, 0, 1024}, {100, 200, 2048}, {100, 200, 4096},
        {-100, -200, 512}, {500, 300, 3072},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        LongRotate(cases[i].x, cases[i].z, cases[i].angle);
        S32 cpp_x = X0, cpp_z = Z0;
        asm_LongRotate(cases[i].x, cases[i].z, cases[i].angle);
        ASSERT_ASM_CPP_EQ_INT(X0, cpp_x, "LongRotate X0");
        ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z, "LongRotate Z0");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_zero_angle);
    RUN_TEST(test_90deg);
    RUN_TEST(test_180deg);
    RUN_TEST(test_360deg);
    RUN_TEST(test_negative_angle);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
