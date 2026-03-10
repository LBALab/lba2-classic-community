/* Test: LongInverseRotatePointF — inverse-rotate; writes X0,Y0,Z0 */
#include "test_harness.h"

#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>
#include <string.h>

extern void LongInverseRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_LongInverseRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z);
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
    LongInverseRotatePointF(&m, 100, 200, 300);
    ASSERT_EQ_INT(100, X0);
    ASSERT_EQ_INT(200, Y0);
    ASSERT_EQ_INT(300, Z0);
}

static void test_zero_point(void)
{
    TYPE_MAT m;
    make_identity(&m);
    LongInverseRotatePointF(&m, 0, 0, 0);
    ASSERT_EQ_INT(0, X0);
    ASSERT_EQ_INT(0, Y0);
    ASSERT_EQ_INT(0, Z0);
}

static void test_transposed_rotation(void)
{
    /* Inverse rotation with identity is just transpose mult */
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    /* 90° rotation around Z: rows become columns for inverse */
    m.F.M11 = 0.0f; m.F.M12 = -1.0f; m.F.M13 = 0.0f;
    m.F.M21 = 1.0f; m.F.M22 = 0.0f;  m.F.M23 = 0.0f;
    m.F.M31 = 0.0f; m.F.M32 = 0.0f;  m.F.M33 = 1.0f;
    LongInverseRotatePointF(&m, 100, 0, 0);
    /* Inverse rot of (100,0,0) by z90: uses column1 = (0,1,0) → X0=0,Y0=100 */
    /* Actually: X0 = M11*x + M21*y + M31*z = 0*100 + 1*0 + 0*0 = 0 */
    /* But wait, this function uses COLUMNS not ROWS for inverse */
    /* LongInverseRotatePointF: X0 = M11*x + M21*y + M31*z */
    ASSERT_EQ_INT(0, X0);
}

static void test_negative_values(void)
{
    TYPE_MAT m;
    make_identity(&m);
    LongInverseRotatePointF(&m, -100, -200, -300);
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
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        LongInverseRotatePointF(&m, cases[i].x, cases[i].y, cases[i].z);
        S32 cpp_x = X0, cpp_y = Y0, cpp_z = Z0;

        asm_LongInverseRotatePointF(&m, cases[i].x, cases[i].y, cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(X0, cpp_x, "LongInverseRotatePointF X0");
        ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y, "LongInverseRotatePointF Y0");
        ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z, "LongInverseRotatePointF Z0");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_identity);
    RUN_TEST(test_zero_point);
    RUN_TEST(test_transposed_rotation);
    RUN_TEST(test_negative_values);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
