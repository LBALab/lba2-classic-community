/* Test: LongRotatePointF — 3D matrix*point; writes X0,Y0,Z0 */
#include "test_harness.h"

#include <3D/ROT3D.H>
#include <3D/CAMERA.H>
#include <string.h>

extern void LongRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_LongRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z);
#endif

static void make_identity(TYPE_MAT *m)
{
    memset(m, 0, sizeof(TYPE_MAT));
    m->F.M11 = 1.0f; m->F.M22 = 1.0f; m->F.M33 = 1.0f;
}

static void test_identity(void)
{
    TYPE_MAT m;
    make_identity(&m);
    LongRotatePointF(&m, 100, 200, 300);
    ASSERT_EQ_INT(100, X0);
    ASSERT_EQ_INT(200, Y0);
    ASSERT_EQ_INT(300, Z0);
}

static void test_zero_point(void)
{
    TYPE_MAT m;
    make_identity(&m);
    LongRotatePointF(&m, 0, 0, 0);
    ASSERT_EQ_INT(0, X0);
    ASSERT_EQ_INT(0, Y0);
    ASSERT_EQ_INT(0, Z0);
}

static void test_scale_matrix(void)
{
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 2.0f; m.F.M22 = 3.0f; m.F.M33 = 4.0f;
    LongRotatePointF(&m, 10, 10, 10);
    ASSERT_EQ_INT(20, X0);
    ASSERT_EQ_INT(30, Y0);
    ASSERT_EQ_INT(40, Z0);
}

static void test_negative_values(void)
{
    TYPE_MAT m;
    make_identity(&m);
    LongRotatePointF(&m, -100, -200, -300);
    ASSERT_EQ_INT(-100, X0);
    ASSERT_EQ_INT(-200, Y0);
    ASSERT_EQ_INT(-300, Z0);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 0.5f; m.F.M12 = 0.3f; m.F.M13 = 0.1f;
    m.F.M21 = 0.2f; m.F.M22 = 0.8f; m.F.M23 = 0.4f;
    m.F.M31 = 0.7f; m.F.M32 = 0.6f; m.F.M33 = 0.9f;

    struct { S32 x, y, z; } cases[] = {
        {0, 0, 0}, {100, 200, 300}, {-100, -200, -300},
        {1000, 0, 0}, {0, 1000, 0}, {0, 0, 1000},
        {-5000, 3000, -1000},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        LongRotatePointF(&m, cases[i].x, cases[i].y, cases[i].z);
        S32 cpp_x = X0, cpp_y = Y0, cpp_z = Z0;
        asm_LongRotatePointF(&m, cases[i].x, cases[i].y, cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(X0, cpp_x, "LongRotatePointF X0");
        ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y, "LongRotatePointF Y0");
        ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z, "LongRotatePointF Z0");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_identity);
    RUN_TEST(test_zero_point);
    RUN_TEST(test_scale_matrix);
    RUN_TEST(test_negative_values);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
