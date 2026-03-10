/* Test: MulMatrixF — 3×3 matrix multiplication (float) */
#include "test_harness.h"

#include <3D/MULMAT.H>
#include <string.h>

extern void MulMatrixF(TYPE_MAT *MatDst, TYPE_MAT *MatSrc1, TYPE_MAT *MatSrc2);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_MulMatrixF(TYPE_MAT *MatDst, TYPE_MAT *MatSrc1, TYPE_MAT *MatSrc2);
#endif

static void make_identity(TYPE_MAT *m)
{
    memset(m, 0, sizeof(TYPE_MAT));
    m->F.M11 = 1.0f; m->F.M22 = 1.0f; m->F.M33 = 1.0f;
}

static void test_identity_times_identity(void)
{
    TYPE_MAT a, b, dst;
    make_identity(&a);
    make_identity(&b);
    MulMatrixF(&dst, &a, &b);
    ASSERT_NEAR_F(1.0f, dst.F.M11, 0.001f);
    ASSERT_NEAR_F(1.0f, dst.F.M22, 0.001f);
    ASSERT_NEAR_F(1.0f, dst.F.M33, 0.001f);
    ASSERT_NEAR_F(0.0f, dst.F.M12, 0.001f);
    ASSERT_NEAR_F(0.0f, dst.F.M13, 0.001f);
}

static void test_identity_times_arb(void)
{
    TYPE_MAT id, arb, dst;
    make_identity(&id);
    memset(&arb, 0, sizeof(arb));
    arb.F.M11 = 2.0f; arb.F.M12 = 3.0f; arb.F.M13 = 4.0f;
    arb.F.M21 = 5.0f; arb.F.M22 = 6.0f; arb.F.M23 = 7.0f;
    arb.F.M31 = 8.0f; arb.F.M32 = 9.0f; arb.F.M33 = 10.0f;
    MulMatrixF(&dst, &id, &arb);
    ASSERT_NEAR_F(2.0f, dst.F.M11, 0.001f);
    ASSERT_NEAR_F(6.0f, dst.F.M22, 0.001f);
    ASSERT_NEAR_F(10.0f, dst.F.M33, 0.001f);
}

static void test_known_product(void)
{
    /* A = [[1,2,0],[0,1,0],[0,0,1]], B = [[1,0,0],[3,1,0],[0,0,1]]
       A*B = [[7,2,0],[3,1,0],[0,0,1]] */
    TYPE_MAT a, b, dst;
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    a.F.M11=1; a.F.M12=2; a.F.M13=0;
    a.F.M21=0; a.F.M22=1; a.F.M23=0;
    a.F.M31=0; a.F.M32=0; a.F.M33=1;
    b.F.M11=1; b.F.M12=0; b.F.M13=0;
    b.F.M21=3; b.F.M22=1; b.F.M23=0;
    b.F.M31=0; b.F.M32=0; b.F.M33=1;
    MulMatrixF(&dst, &a, &b);
    ASSERT_NEAR_F(7.0f, dst.F.M11, 0.01f);
    ASSERT_NEAR_F(2.0f, dst.F.M12, 0.01f);
    ASSERT_NEAR_F(3.0f, dst.F.M21, 0.01f);
    ASSERT_NEAR_F(1.0f, dst.F.M22, 0.01f);
    ASSERT_NEAR_F(1.0f, dst.F.M33, 0.01f);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    TYPE_MAT a, b, cpp_dst, asm_dst;
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    a.F.M11=1.5f; a.F.M12=2.3f; a.F.M13=0.7f;
    a.F.M21=0.1f; a.F.M22=3.2f; a.F.M23=1.1f;
    a.F.M31=2.0f; a.F.M32=0.5f; a.F.M33=4.0f;
    b.F.M11=0.9f; b.F.M12=1.2f; b.F.M13=0.3f;
    b.F.M21=2.1f; b.F.M22=0.4f; b.F.M23=1.5f;
    b.F.M31=0.6f; b.F.M32=3.0f; b.F.M33=0.8f;
    MulMatrixF(&cpp_dst, &a, &b);
    asm_MulMatrixF(&asm_dst, &a, &b);
    for (int j = 0; j < 9; j++) {
        ASSERT_ASM_CPP_NEAR_F((&asm_dst.F.M11)[j], (&cpp_dst.F.M11)[j], 0.01f, "MulMatrixF");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_identity_times_identity);
    RUN_TEST(test_identity_times_arb);
    RUN_TEST(test_known_product);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
