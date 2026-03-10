/* Test: CopyMatrixF — copy TYPE_MAT (float 3×3 rotation matrix) */
#include "test_harness.h"

#include <3D/COPYMAT.H>
#include <string.h>

/* CopyMatrixF is not in the header — only CopyMatrix (pointer). Declare it. */
extern void CopyMatrixF(TYPE_MAT *MatDst, TYPE_MAT *MatSrc);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_CopyMatrixF(TYPE_MAT *MatDst, TYPE_MAT *MatSrc);
#endif

static void make_identity(TYPE_MAT *m)
{
    memset(m, 0, sizeof(TYPE_MAT));
    m->F.M11 = 1.0f; m->F.M22 = 1.0f; m->F.M33 = 1.0f;
}

static void make_arbitrary(TYPE_MAT *m)
{
    m->F.M11 = 1.1f; m->F.M12 = 2.2f; m->F.M13 = 3.3f;
    m->F.M21 = 4.4f; m->F.M22 = 5.5f; m->F.M23 = 6.6f;
    m->F.M31 = 7.7f; m->F.M32 = 8.8f; m->F.M33 = 9.9f;
    m->F.TX  = 10.0f; m->F.TY = 11.0f; m->F.TZ = 12.0f;
}

static void test_copy_identity(void)
{
    TYPE_MAT src, dst;
    memset(&dst, 0xFF, sizeof(dst));
    make_identity(&src);
    CopyMatrixF(&dst, &src);
    ASSERT_MEM_EQ(&src, &dst, sizeof(TYPE_MAT));
}

static void test_copy_arbitrary(void)
{
    TYPE_MAT src, dst;
    memset(&dst, 0, sizeof(dst));
    make_arbitrary(&src);
    CopyMatrixF(&dst, &src);
    ASSERT_MEM_EQ(&src, &dst, sizeof(TYPE_MAT));
}

static void test_copy_zero(void)
{
    TYPE_MAT src, dst;
    memset(&src, 0, sizeof(src));
    memset(&dst, 0xFF, sizeof(dst));
    CopyMatrixF(&dst, &src);
    ASSERT_MEM_EQ(&src, &dst, sizeof(TYPE_MAT));
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    TYPE_MAT src, dst_cpp, dst_asm;
    make_arbitrary(&src);

    memset(&dst_cpp, 0, sizeof(dst_cpp));
    CopyMatrixF(&dst_cpp, &src);

    memset(&dst_asm, 0, sizeof(dst_asm));
    asm_CopyMatrixF(&dst_asm, &src);

    ASSERT_ASM_CPP_MEM_EQ(&dst_asm, &dst_cpp, sizeof(TYPE_MAT), "CopyMatrixF");
}
#endif

int main(void)
{
    RUN_TEST(test_copy_identity);
    RUN_TEST(test_copy_arbitrary);
    RUN_TEST(test_copy_zero);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
