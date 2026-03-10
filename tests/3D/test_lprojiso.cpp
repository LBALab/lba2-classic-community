/* Test: LongProjectPointIso — isometric projection; writes Xp, Yp */
#include "test_harness.h"

#include <3D/LPROJ.H>
#include <3D/CAMERA.H>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_LongProjectPointIso(S32 x, S32 y, S32 z);
#endif

static void test_at_camera(void)
{
    CameraX = 0; CameraY = 0; CameraZ = 0;
    XCentre = 320; YCentre = 240;
    LongProjectPointIso(0, 0, 0);
    ASSERT_EQ_INT(320, Xp);
    /* Yp = YCentre + 1 for iso */
    ASSERT_EQ_INT(241, Yp);
}

static void test_offset_x(void)
{
    CameraX = 0; CameraY = 0; CameraZ = 0;
    XCentre = 320; YCentre = 240;
    LongProjectPointIso(64, 0, 0);
    /* x=64,z=0 → newX = 320 + ((64-0)*3/64) = 320+3 = 323 */
    ASSERT_EQ_INT(323, Xp);
}

static void test_with_camera_offset(void)
{
    CameraX = 100; CameraY = 0; CameraZ = 0;
    XCentre = 320; YCentre = 240;
    LongProjectPointIso(100, 0, 0);
    /* x-cameraX = 0 → Xp = 320 */
    ASSERT_EQ_INT(320, Xp);
}

static void test_negative_coords(void)
{
    CameraX = 0; CameraY = 0; CameraZ = 0;
    XCentre = 320; YCentre = 240;
    LongProjectPointIso(-64, 0, 0);
    ASSERT_EQ_INT(317, Xp);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    CameraX = 0; CameraY = 0; CameraZ = 0;
    XCentre = 320; YCentre = 240;
    struct { S32 x, y, z; } cases[] = {
        {0,0,0}, {64,0,0}, {0,64,0}, {0,0,64},
        {100,50,200}, {-100,-50,-200},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        LongProjectPointIso(cases[i].x, cases[i].y, cases[i].z);
        S32 cpp_xp = Xp, cpp_yp = Yp;
        asm_LongProjectPointIso(cases[i].x, cases[i].y, cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(Xp, cpp_xp, "LongProjectPointIso Xp");
        ASSERT_ASM_CPP_EQ_INT(Yp, cpp_yp, "LongProjectPointIso Yp");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_at_camera);
    RUN_TEST(test_offset_x);
    RUN_TEST(test_with_camera_offset);
    RUN_TEST(test_negative_coords);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
