/* Test: RotateMatrixU — rotate matrix by Euler angles */
#include "test_harness.h"

#include <3D/ROTMAT.H>
#include <3D/IMATSTD.H>
#include <string.h>

extern void RotateMatrixU(TYPE_MAT *MatDst, TYPE_MAT *MatSrc, S32 x, S32 y, S32 z);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_RotateMatrixU(TYPE_MAT *MatDst, TYPE_MAT *MatSrc, S32 x, S32 y, S32 z);
#endif

static void make_identity(TYPE_MAT *m)
{
    memset(m, 0, sizeof(TYPE_MAT));
    m->F.M11 = 1.0f; m->F.M22 = 1.0f; m->F.M33 = 1.0f;
}

static void test_zero_rotation(void)
{
    TYPE_MAT src, dst;
    make_identity(&src);
    RotateMatrixU(&dst, &src, 0, 0, 0);
    ASSERT_NEAR_F(1.0f, dst.F.M11, 0.01f);
    ASSERT_NEAR_F(1.0f, dst.F.M22, 0.01f);
    ASSERT_NEAR_F(1.0f, dst.F.M33, 0.01f);
}

static void test_90deg(void)
{
    TYPE_MAT src, dst;
    make_identity(&src);
    RotateMatrixU(&dst, &src, 0, 1024, 0);
    /* Should be equivalent to a 90° Y rotation */
    ASSERT_NEAR_F(0.0f, dst.F.M11, 0.02f);
    ASSERT_NEAR_F(1.0f, dst.F.M13, 0.02f);
}

static void test_non_identity_src(void)
{
    TYPE_MAT src, dst;
    memset(&src, 0, sizeof(src));
    src.F.M11 = 2.0f; src.F.M22 = 2.0f; src.F.M33 = 2.0f;
    RotateMatrixU(&dst, &src, 0, 0, 0);
    ASSERT_NEAR_F(2.0f, dst.F.M11, 0.01f);
    ASSERT_NEAR_F(2.0f, dst.F.M22, 0.01f);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    TYPE_MAT src, cpp_dst, asm_dst;
    make_identity(&src);
    struct { S32 a, b, g; } cases[] = {
        {0, 0, 0}, {1024, 0, 0}, {0, 1024, 0}, {0, 0, 1024},
        {512, 512, 512},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        RotateMatrixU(&cpp_dst, &src, cases[i].a, cases[i].b, cases[i].g);
        asm_RotateMatrixU(&asm_dst, &src, cases[i].a, cases[i].b, cases[i].g);
        for (int j = 0; j < 9; j++) {
            ASSERT_ASM_CPP_NEAR_F((&asm_dst.F.M11)[j], (&cpp_dst.F.M11)[j], 0.01f, "RotateMatrixU");
        }
    }
}
#endif

int main(void)
{
    RUN_TEST(test_zero_rotation);
    RUN_TEST(test_90deg);
    RUN_TEST(test_non_identity_src);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
