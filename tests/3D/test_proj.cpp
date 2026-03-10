/* Test: SetProjection, SetIsoProjection — configure projection globals */
#include "test_harness.h"

#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <3D/LPROJ.H>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_SetProjection(S32 xc, S32 yc, S32 clip, S32 fx, S32 fy);
extern "C" void asm_SetIsoProjection(S32 xc, S32 yc);
#endif

static void test_set_projection(void)
{
    SetProjection(320, 240, 10, 500, 400);
    ASSERT_EQ_INT(320, XCentre);
    ASSERT_EQ_INT(240, YCentre);
    ASSERT_EQ_INT(10, NearClip);
    ASSERT_EQ_INT(500, LFactorX);
    ASSERT_EQ_INT(400, LFactorY);
    ASSERT_EQ_INT(TYPE_3D, TypeProj);
}

static void test_set_iso_projection(void)
{
    SetIsoProjection(160, 120);
    ASSERT_EQ_INT(160, XCentre);
    ASSERT_EQ_INT(120, YCentre);
    ASSERT_EQ_INT(TYPE_ISO, TypeProj);
}

static void test_projection_func_pointers(void)
{
    SetProjection(320, 240, 1, 100, 100);
    ASSERT_TRUE(LongProjectPoint != NULL);
    ASSERT_TRUE(ProjectList != NULL);
}

static void test_iso_func_pointers(void)
{
    SetIsoProjection(320, 240);
    ASSERT_TRUE(LongProjectPoint != NULL);
    ASSERT_TRUE(ProjectList != NULL);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    asm_SetProjection(320, 240, 10, 500, 400);
    S32 asm_xc = XCentre, asm_yc = YCentre, asm_nc = NearClip;
    S32 asm_fx = LFactorX, asm_fy = LFactorY, asm_tp = TypeProj;

    SetProjection(320, 240, 10, 500, 400);
    ASSERT_ASM_CPP_EQ_INT(asm_xc, XCentre, "SetProjection XCentre");
    ASSERT_ASM_CPP_EQ_INT(asm_yc, YCentre, "SetProjection YCentre");
    ASSERT_ASM_CPP_EQ_INT(asm_nc, NearClip, "SetProjection NearClip");
    ASSERT_ASM_CPP_EQ_INT(asm_fx, LFactorX, "SetProjection LFactorX");
    ASSERT_ASM_CPP_EQ_INT(asm_fy, LFactorY, "SetProjection LFactorY");
    ASSERT_ASM_CPP_EQ_INT(asm_tp, TypeProj, "SetProjection TypeProj");
}
#endif

int main(void)
{
    RUN_TEST(test_set_projection);
    RUN_TEST(test_set_iso_projection);
    RUN_TEST(test_projection_func_pointers);
    RUN_TEST(test_iso_func_pointers);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
