/* Test: LightList — per-vertex lighting from normals */
#include "test_harness.h"

#include <3D/LITLISTF.H>
#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_LightList(TYPE_MAT *Mat, U16 *dst, TYPE_VT16 *src, S32 n);
#endif

static void make_identity(TYPE_MAT *m)
{
    memset(m, 0, sizeof(TYPE_MAT));
    m->F.M11 = 1.0f; m->F.M22 = 1.0f; m->F.M33 = 1.0f;
}

static void test_zero_vertices(void)
{
    TYPE_MAT m;
    make_identity(&m);
    U16 dst[1] = {0xFFFF};
    TYPE_VT16 src[1] = {{0, 0, 1, 0}};
    LightList(&m, dst, src, 0);
    /* Nothing should change */
    ASSERT_EQ_UINT(0xFFFF, dst[0]);
}

static void test_single_vertex(void)
{
    TYPE_MAT m;
    make_identity(&m);
    CameraXLight = 0; CameraYLight = 0; CameraZLight = 100;
    FactorLight = 1.0f;
    U16 dst[1] = {0};
    TYPE_VT16 src[1] = {{0, 0, 100, 0}};
    LightList(&m, dst, src, 1);
    /* Light value should be positive */
    ASSERT_TRUE(dst[0] > 0);
}

static void test_perpendicular_normal(void)
{
    TYPE_MAT m;
    make_identity(&m);
    CameraXLight = 100; CameraYLight = 0; CameraZLight = 0;
    FactorLight = 1.0f;
    U16 dst[1] = {0};
    TYPE_VT16 src[1] = {{0, 100, 0, 0}};  /* Normal perpendicular to light */
    LightList(&m, dst, src, 1);
    /* Dot product = 0 → light value should be 0 */
    ASSERT_EQ_UINT(0, dst[0]);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    TYPE_MAT m;
    make_identity(&m);
    CameraXLight = 50; CameraYLight = 50; CameraZLight = 50;
    FactorLight = 0.01f;
    TYPE_VT16 src[3] = {{100,0,0,0}, {0,100,0,0}, {0,0,100,0}};
    U16 dst_cpp[3], dst_asm[3];
    memset(dst_cpp, 0, sizeof(dst_cpp));
    memset(dst_asm, 0, sizeof(dst_asm));
    LightList(&m, dst_cpp, src, 3);
    asm_LightList(&m, dst_asm, src, 3);
    ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp, sizeof(dst_cpp), "LightList");
}
#endif

int main(void)
{
    RUN_TEST(test_zero_vertices);
    RUN_TEST(test_single_vertex);
    RUN_TEST(test_perpendicular_normal);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
