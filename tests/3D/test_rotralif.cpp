/* Test: RotTransListF — batch rotate+translate vertex list */
#include "test_harness.h"

#include <3D/ROTRALIS.H>
#include <string.h>

extern void RotTransListF(TYPE_MAT *Mat, TYPE_VT16 *Dst, TYPE_VT16 *Src, S32 NbPoints);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_RotTransListF(TYPE_MAT *Mat, TYPE_VT16 *Dst, TYPE_VT16 *Src, S32 NbPoints);
#endif

static void make_identity_with_trans(TYPE_MAT *m, float tx, float ty, float tz)
{
    memset(m, 0, sizeof(TYPE_MAT));
    m->F.M11 = 1.0f; m->F.M22 = 1.0f; m->F.M33 = 1.0f;
    m->F.TX = tx; m->F.TY = ty; m->F.TZ = tz;
}

static void test_identity_no_trans(void)
{
    TYPE_MAT m;
    make_identity_with_trans(&m, 0, 0, 0);
    TYPE_VT16 src = {100, 200, 300, 0};
    TYPE_VT16 dst = {0, 0, 0, 0};
    RotTransListF(&m, &dst, &src, 1);
    ASSERT_EQ_INT(100, dst.X);
    ASSERT_EQ_INT(200, dst.Y);
    ASSERT_EQ_INT(300, dst.Z);
}

static void test_translation_only(void)
{
    TYPE_MAT m;
    make_identity_with_trans(&m, 10, 20, 30);
    TYPE_VT16 src = {100, 200, 300, 0};
    TYPE_VT16 dst = {0, 0, 0, 0};
    RotTransListF(&m, &dst, &src, 1);
    ASSERT_EQ_INT(110, dst.X);
    ASSERT_EQ_INT(220, dst.Y);
    ASSERT_EQ_INT(330, dst.Z);
}

static void test_multiple_points(void)
{
    TYPE_MAT m;
    make_identity_with_trans(&m, 0, 0, 0);
    TYPE_VT16 src[3] = {{10,20,30,0}, {40,50,60,0}, {70,80,90,0}};
    TYPE_VT16 dst[3];
    memset(dst, 0, sizeof(dst));
    RotTransListF(&m, dst, src, 3);
    ASSERT_EQ_INT(10, dst[0].X);
    ASSERT_EQ_INT(50, dst[1].Y);
    ASSERT_EQ_INT(90, dst[2].Z);
}

static void test_zero_points(void)
{
    TYPE_MAT m;
    make_identity_with_trans(&m, 0, 0, 0);
    TYPE_VT16 src = {100, 200, 300, 0};
    TYPE_VT16 dst = {99, 99, 99, 0};
    RotTransListF(&m, &dst, &src, 0);
    /* Zero points means nothing should be written */
    ASSERT_EQ_INT(99, dst.X);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 0.5f; m.F.M12 = 0.3f; m.F.M13 = 0.1f;
    m.F.M21 = 0.2f; m.F.M22 = 0.8f; m.F.M23 = 0.4f;
    m.F.M31 = 0.7f; m.F.M32 = 0.6f; m.F.M33 = 0.9f;
    m.F.TX = 5.0f; m.F.TY = 10.0f; m.F.TZ = 15.0f;

    TYPE_VT16 src[3] = {{100,200,300,0}, {-50,0,50,0}, {0,0,0,0}};
    TYPE_VT16 dst_cpp[3], dst_asm[3];
    memset(dst_cpp, 0, sizeof(dst_cpp));
    memset(dst_asm, 0, sizeof(dst_asm));
    RotTransListF(&m, dst_cpp, src, 3);
    asm_RotTransListF(&m, dst_asm, src, 3);
    ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp, sizeof(dst_cpp), "RotTransListF");
}
#endif

int main(void)
{
    RUN_TEST(test_identity_no_trans);
    RUN_TEST(test_translation_only);
    RUN_TEST(test_multiple_points);
    RUN_TEST(test_zero_points);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
