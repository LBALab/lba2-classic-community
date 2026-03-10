/* Test: LongProjectPoint3D — single-point 3D perspective projection */
#include "test_harness.h"

#include <3D/LPROJ.H>
#include <3D/PROJ.H>
#include <3D/CAMERA.H>

extern S32 LongProjectPoint3D(S32 x, S32 y, S32 z);

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_LongProjectPoint3D(S32 x, S32 y, S32 z);
#endif

static void setup(void)
{
    SetProjection(320, 240, 1, 300, 300);
    CameraXr = 0; CameraYr = 0; CameraZr = 1000;
    CameraZrClip = CameraZr - NearClip;
}

static void test_point_in_front(void)
{
    setup();
    S32 ret = LongProjectPoint3D(0, 0, 0);
    ASSERT_EQ_INT(1, ret);
    ASSERT_EQ_INT(320, Xp);
    ASSERT_EQ_INT(240, Yp);
}

static void test_point_behind(void)
{
    setup();
    S32 ret = LongProjectPoint3D(0, 0, 2000);
    ASSERT_EQ_INT(0, ret);
}

static void test_off_center_x(void)
{
    setup();
    S32 ret = LongProjectPoint3D(100, 0, 0);
    ASSERT_EQ_INT(1, ret);
    ASSERT_TRUE(Xp > 320);
}

static void test_off_center_y(void)
{
    setup();
    S32 ret = LongProjectPoint3D(0, 100, 0);
    ASSERT_EQ_INT(1, ret);
    ASSERT_TRUE(Yp != 240);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    setup();
    struct { S32 x, y, z; } cases[] = {
        {0, 0, 0}, {100, 0, 0}, {0, 100, 0}, {-100, -100, 0},
        {0, 0, 500}, {0, 0, 2000},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        S32 cpp_ret = LongProjectPoint3D(cases[i].x, cases[i].y, cases[i].z);
        S32 cpp_xp = Xp, cpp_yp = Yp;
        S32 asm_ret = asm_LongProjectPoint3D(cases[i].x, cases[i].y, cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(asm_ret, cpp_ret, "LongProjectPoint3D ret");
        if (cpp_ret == 1) {
            ASSERT_ASM_CPP_EQ_INT(Xp, cpp_xp, "LongProjectPoint3D Xp");
            ASSERT_ASM_CPP_EQ_INT(Yp, cpp_yp, "LongProjectPoint3D Yp");
        }
    }
}
#endif

int main(void)
{
    RUN_TEST(test_point_in_front);
    RUN_TEST(test_point_behind);
    RUN_TEST(test_off_center_x);
    RUN_TEST(test_off_center_y);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
