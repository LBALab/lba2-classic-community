/* Test: GetAngleVector3D — 3D angle from (x,y,z); writes X0, Y0 */
#include "test_harness.h"

#include <3D/GETANG3D.H>
#include <3D/CAMERA.H>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_GetAngleVector3D(S32 x, S32 y, S32 z);
#endif

static void test_along_z(void)
{
    GetAngleVector3D(0, 0, 100);
    /* Looking along +Z: Y0 (horizontal) should be ~0, X0 (vertical) ~0 */
    ASSERT_TRUE(X0 >= 0 && X0 < 100);
}

static void test_along_x(void)
{
    GetAngleVector3D(100, 0, 0);
    /* Y0 should indicate ~90° = 1024 */
    ASSERT_TRUE(Y0 > 900 && Y0 < 1200);
}

static void test_straight_up(void)
{
    GetAngleVector3D(0, 100, 0);
    /* X0 should indicate roughly -90° vertical elevation */
    ASSERT_TRUE(X0 > 2800 || X0 < 200);
}

static void test_negative_values(void)
{
    GetAngleVector3D(-100, -50, -200);
    /* Just ensure it returns without crashing and values are in [0,4095] */
    ASSERT_TRUE((X0 & 4095) == X0);
    ASSERT_TRUE((Y0 & 4095) == Y0);
}

static void test_zero(void)
{
    /* (0,0,0) — degenerate, just ensure no crash */
    GetAngleVector3D(0, 0, 0);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    struct { S32 x, y, z; } cases[] = {
        {0, 0, 100}, {100, 0, 0}, {0, 100, 0},
        {-100, -50, -200}, {0, 0, 0},
        {1, 1, 1}, {1000, 500, 200}, {-1, 0, 1},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        S32 cpp_ret = GetAngleVector3D(cases[i].x, cases[i].y, cases[i].z);
        S32 cpp_x0 = X0, cpp_y0 = Y0;

        S32 asm_ret = asm_GetAngleVector3D(cases[i].x, cases[i].y, cases[i].z);
        S32 asm_x0 = X0, asm_y0 = Y0;

        ASSERT_ASM_CPP_EQ_INT(asm_ret, cpp_ret, "GetAngleVector3D ret");
        ASSERT_ASM_CPP_EQ_INT(asm_x0, cpp_x0, "GetAngleVector3D X0");
        ASSERT_ASM_CPP_EQ_INT(asm_y0, cpp_y0, "GetAngleVector3D Y0");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_along_z);
    RUN_TEST(test_along_x);
    RUN_TEST(test_straight_up);
    RUN_TEST(test_negative_values);
    RUN_TEST(test_zero);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
