/* Test: InitMatrixTransF — set matrix translation fields */
#include "test_harness.h"

#include <3D/IMATTRA.H>
#include <string.h>

extern void InitMatrixTransF(TYPE_MAT *Mat, S32 tx, S32 ty, S32 tz);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_InitMatrixTransF(TYPE_MAT *Mat, S32 tx, S32 ty, S32 tz);
#endif

static void test_set_translation(void)
{
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    InitMatrixTransF(&m, 100, 200, 300);
    ASSERT_NEAR_F(100.0f, m.F.TX, 0.01f);
    ASSERT_NEAR_F(200.0f, m.F.TY, 0.01f);
    ASSERT_NEAR_F(300.0f, m.F.TZ, 0.01f);
}

static void test_zero_translation(void)
{
    TYPE_MAT m;
    memset(&m, 0xFF, sizeof(m));
    InitMatrixTransF(&m, 0, 0, 0);
    ASSERT_NEAR_F(0.0f, m.F.TX, 0.01f);
    ASSERT_NEAR_F(0.0f, m.F.TY, 0.01f);
    ASSERT_NEAR_F(0.0f, m.F.TZ, 0.01f);
}

static void test_negative_translation(void)
{
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    InitMatrixTransF(&m, -500, -1000, -2000);
    ASSERT_NEAR_F(-500.0f, m.F.TX, 0.01f);
    ASSERT_NEAR_F(-1000.0f, m.F.TY, 0.01f);
    ASSERT_NEAR_F(-2000.0f, m.F.TZ, 0.01f);
}

static void test_rotation_untouched(void)
{
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 1.0f; m.F.M22 = 1.0f; m.F.M33 = 1.0f;
    InitMatrixTransF(&m, 10, 20, 30);
    /* Rotation part should be untouched */
    ASSERT_NEAR_F(1.0f, m.F.M11, 0.001f);
    ASSERT_NEAR_F(1.0f, m.F.M22, 0.001f);
    ASSERT_NEAR_F(1.0f, m.F.M33, 0.001f);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    struct { S32 tx, ty, tz; } cases[] = {
        {0, 0, 0}, {100, 200, 300}, {-500, -1000, -2000},
        {2147483647, -2147483647, 0},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        TYPE_MAT cpp_m, asm_m;
        memset(&cpp_m, 0, sizeof(cpp_m));
        memset(&asm_m, 0, sizeof(asm_m));
        InitMatrixTransF(&cpp_m, cases[i].tx, cases[i].ty, cases[i].tz);
        asm_InitMatrixTransF(&asm_m, cases[i].tx, cases[i].ty, cases[i].tz);
        ASSERT_ASM_CPP_MEM_EQ(&asm_m, &cpp_m, sizeof(TYPE_MAT), "InitMatrixTransF");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_set_translation);
    RUN_TEST(test_zero_translation);
    RUN_TEST(test_negative_translation);
    RUN_TEST(test_rotation_untouched);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
