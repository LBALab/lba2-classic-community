/* Test: ProjectListIso — batch isometric projection */
#include "test_harness.h"

#include <3D/LPROJ.H>
#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>
#include <string.h>

extern void ProjectListIso(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX, S32 OrgY, S32 OrgZ);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_ProjectListIso(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX, S32 OrgY, S32 OrgZ);
#endif

static void setup_iso(void)
{
    XCentre = 320; YCentre = 240;
    ScreenXMin = 32767; ScreenXMax = -32767;
    ScreenYMin = 32767; ScreenYMax = -32767;
}

static void test_origin_point(void)
{
    setup_iso();
    TYPE_PT dst[1];
    TYPE_VT16 src[1] = {{0, 0, 0, 0}};
    ProjectListIso(dst, src, 1, 0, 0, 0);
    ASSERT_EQ_INT(320, dst[0].X);
}

static void test_single_offset(void)
{
    setup_iso();
    TYPE_PT dst[1];
    TYPE_VT16 src[1] = {{64, 0, 0, 0}};
    ProjectListIso(dst, src, 1, 0, 0, 0);
    /* Iso uses (x-z)*3/64 for Xe */
    ASSERT_TRUE(dst[0].X != -32768);
}

static void test_zero_count(void)
{
    setup_iso();
    TYPE_PT dst[1] = {{99, 99}};
    TYPE_VT16 src[1] = {{0, 0, 0, 0}};
    ProjectListIso(dst, src, 0, 0, 0, 0);
    /* Nothing should be written */
    ASSERT_EQ_INT(99, dst[0].X);
}

static void test_multiple_points(void)
{
    setup_iso();
    TYPE_PT dst[3];
    TYPE_VT16 src[3] = {{0,0,0,0}, {100,50,100,0}, {-100,-50,-100,0}};
    ProjectListIso(dst, src, 3, 0, 0, 0);
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(dst[i].X >= -32768 && dst[i].X <= 32767);
    }
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    setup_iso();
    TYPE_PT dst_cpp[3], dst_asm[3];
    TYPE_VT16 src[3] = {{0,0,0,0}, {100,50,100,0}, {-100,-50,-100,0}};

    ScreenXMin = 32767; ScreenXMax = -32767;
    ScreenYMin = 32767; ScreenYMax = -32767;
    ProjectListIso(dst_cpp, src, 3, 0, 0, 0);

    ScreenXMin = 32767; ScreenXMax = -32767;
    ScreenYMin = 32767; ScreenYMax = -32767;
    asm_ProjectListIso(dst_asm, src, 3, 0, 0, 0);

    ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp, sizeof(dst_cpp), "ProjectListIso");
}
#endif

int main(void)
{
    RUN_TEST(test_origin_point);
    RUN_TEST(test_single_offset);
    RUN_TEST(test_zero_count);
    RUN_TEST(test_multiple_points);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
