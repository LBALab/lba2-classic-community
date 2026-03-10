/* Test: ProjectList3DF (from PRLI3DF) — batch 3D perspective projection */
#include "test_harness.h"

#include <3D/LPROJ.H>
#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>
#include <string.h>

extern void ProjectList3DF(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX, S32 OrgY, S32 OrgZ);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_ProjectList3DF(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX, S32 OrgY, S32 OrgZ);
#endif

static void setup_projection(void)
{
    SetProjection(320, 240, 1, 300, 300);
    ScreenXMin = 32767; ScreenXMax = -32767;
    ScreenYMin = 32767; ScreenYMax = -32767;
}

static void test_zero_points(void)
{
    setup_projection();
    TYPE_PT dst[1];
    TYPE_VT16 src[1] = {{0,0,0,0}};
    ProjectList3DF(dst, src, 0, 0, 0, 0);
}

static void test_single_point(void)
{
    setup_projection();
    TYPE_PT dst[1];
    TYPE_VT16 src[1] = {{0, 0, -100, 0}};
    ProjectList3DF(dst, src, 1, 0, 0, 0);
    ASSERT_TRUE(dst[0].X != -32768);
}

static void test_behind_camera(void)
{
    setup_projection();
    TYPE_PT dst[1];
    TYPE_VT16 src[1] = {{0, 0, 100, 0}};
    ProjectList3DF(dst, src, 1, 0, 0, 0);
    ASSERT_EQ_INT(-32768, dst[0].X);
    ASSERT_EQ_INT(-32768, dst[0].Y);
}

static void test_multiple_points(void)
{
    setup_projection();
    TYPE_PT dst[3];
    TYPE_VT16 src[3] = {{0,0,-50,0}, {10,10,-100,0}, {-10,-10,-200,0}};
    ProjectList3DF(dst, src, 3, 0, 0, 0);
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(dst[i].X != -32768 || dst[i].Y != -32768);
    }
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    setup_projection();
    TYPE_PT dst_cpp[3], dst_asm[3];
    TYPE_VT16 src[3] = {{0,0,-50,0}, {10,10,-100,0}, {-10,-10,-200,0}};

    memset(dst_cpp, 0, sizeof(dst_cpp));
    ScreenXMin = 32767; ScreenXMax = -32767;
    ScreenYMin = 32767; ScreenYMax = -32767;
    ProjectList3DF(dst_cpp, src, 3, 0, 0, 0);

    memset(dst_asm, 0, sizeof(dst_asm));
    ScreenXMin = 32767; ScreenXMax = -32767;
    ScreenYMin = 32767; ScreenYMax = -32767;
    asm_ProjectList3DF(dst_asm, src, 3, 0, 0, 0);

    ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp, sizeof(dst_cpp), "ProjectList3DF");
}
#endif

int main(void)
{
    RUN_TEST(test_zero_points);
    RUN_TEST(test_single_point);
    RUN_TEST(test_behind_camera);
    RUN_TEST(test_multiple_points);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
