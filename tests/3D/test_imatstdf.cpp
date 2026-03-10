/* Test: InitMatrixStdF — init rotation matrix from Euler angles */
#include "test_harness.h"

#include <3D/IMATSTD.H>
#include <3D/SINTABF.H>
#include <string.h>

extern void InitMatrixStdF(TYPE_MAT *Mat, S32 alpha, S32 beta, S32 gamma);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_InitMatrixStdF(TYPE_MAT *Mat, S32 alpha, S32 beta, S32 gamma);
#endif

static void test_zero_angles(void)
{
    TYPE_MAT m;
    memset(&m, 0xFF, sizeof(m));
    InitMatrixStdF(&m, 0, 0, 0);
    /* Identity rotation: M11=M22=M33=1, rest=0 */
    ASSERT_NEAR_F(1.0f, m.F.M11, 0.001f);
    ASSERT_NEAR_F(0.0f, m.F.M12, 0.001f);
    ASSERT_NEAR_F(0.0f, m.F.M13, 0.001f);
    ASSERT_NEAR_F(0.0f, m.F.M21, 0.001f);
    ASSERT_NEAR_F(1.0f, m.F.M22, 0.001f);
    ASSERT_NEAR_F(0.0f, m.F.M23, 0.001f);
    ASSERT_NEAR_F(0.0f, m.F.M31, 0.001f);
    ASSERT_NEAR_F(0.0f, m.F.M32, 0.001f);
    ASSERT_NEAR_F(1.0f, m.F.M33, 0.001f);
}

static void test_90deg_beta(void)
{
    TYPE_MAT m;
    /* 90° = 1024 in engine units (4096 = 360°) */
    InitMatrixStdF(&m, 0, 1024, 0);
    /* Pure Y rotation 90°: M11≈0, M13≈1, M31≈-1, M33≈0 */
    ASSERT_NEAR_F(0.0f, m.F.M11, 0.01f);
    ASSERT_NEAR_F(1.0f, m.F.M13, 0.01f);
    ASSERT_NEAR_F(-1.0f, m.F.M31, 0.01f);
    ASSERT_NEAR_F(0.0f, m.F.M33, 0.01f);
}

static void test_translation_zeroed(void)
{
    TYPE_MAT m;
    memset(&m, 0xFF, sizeof(m));
    InitMatrixStdF(&m, 500, 1000, 2000);
    ASSERT_NEAR_F(0.0f, m.F.TX, 0.001f);
    ASSERT_NEAR_F(0.0f, m.F.TY, 0.001f);
    ASSERT_NEAR_F(0.0f, m.F.TZ, 0.001f);
}

static void test_full_rotation(void)
{
    /* 360° = 4096 → should be identity again */
    TYPE_MAT m;
    InitMatrixStdF(&m, 4096, 4096, 4096);
    ASSERT_NEAR_F(1.0f, m.F.M11, 0.001f);
    ASSERT_NEAR_F(1.0f, m.F.M22, 0.001f);
    ASSERT_NEAR_F(1.0f, m.F.M33, 0.001f);
}

static void test_angle_wrapping(void)
{
    /* Negative angles should wrap via & 4095 */
    TYPE_MAT m1, m2;
    InitMatrixStdF(&m1, -1024, 0, 0);
    InitMatrixStdF(&m2, 3072, 0, 0);  /* 4096-1024 = 3072 */
    ASSERT_NEAR_F(m1.F.M11, m2.F.M11, 0.001f);
    ASSERT_NEAR_F(m1.F.M22, m2.F.M22, 0.001f);
    ASSERT_NEAR_F(m1.F.M33, m2.F.M33, 0.001f);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    struct { S32 a, b, g; } cases[] = {
        {0, 0, 0}, {1024, 0, 0}, {0, 1024, 0}, {0, 0, 1024},
        {512, 512, 512}, {2048, 2048, 2048}, {4096, 4096, 4096},
        {-1024, 500, 3000},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        TYPE_MAT cpp_m, asm_m;
        InitMatrixStdF(&cpp_m, cases[i].a, cases[i].b, cases[i].g);
        asm_InitMatrixStdF(&asm_m, cases[i].a, cases[i].b, cases[i].g);
        ASSERT_ASM_CPP_MEM_EQ(&asm_m, &cpp_m, sizeof(TYPE_MAT), "InitMatrixStdF");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_zero_angles);
    RUN_TEST(test_90deg_beta);
    RUN_TEST(test_translation_zeroed);
    RUN_TEST(test_full_rotation);
    RUN_TEST(test_angle_wrapping);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
