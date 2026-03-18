/* Test: LongInverseRotatePointF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>
#include <string.h>
#include <stdlib.h>

extern void LongInverseRotatePointF(TYPE_MAT *m, S32 x, S32 y, S32 z);
extern "C" void asm_LongInverseRotatePointF(void);
static void call_asm_LongInverseRotatePointF(TYPE_MAT *m, S32 x, S32 y, S32 z) {
    __asm__ __volatile__("call asm_LongInverseRotatePointF" : : "S"(m), "a"(x), "b"(y), "c"(z) : "memory");
}

static void test_equivalence(void)
{
    TYPE_MAT m; memset(&m,0,sizeof(m));
    m.F.M11=0.5f;m.F.M12=0.3f;m.F.M13=0.1f;
    m.F.M21=0.2f;m.F.M22=0.8f;m.F.M23=0.4f;
    m.F.M31=0.7f;m.F.M32=0.6f;m.F.M33=0.9f;
    struct { S32 x,y,z; } cases[] = {
        {0,0,0},{100,200,300},{-100,-200,-300},{1000,0,0},{0,1000,0},{0,0,1000},
    };
    for (int i=0;i<(int)(sizeof(cases)/sizeof(cases[0]));i++) {
        LongInverseRotatePointF(&m,cases[i].x,cases[i].y,cases[i].z);
        S32 cx=X0,cy=Y0,cz=Z0;
        call_asm_LongInverseRotatePointF(&m,cases[i].x,cases[i].y,cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(X0,cx,"LongInverseRotatePointF X0");
        ASSERT_ASM_CPP_EQ_INT(Y0,cy,"LongInverseRotatePointF Y0");
        ASSERT_ASM_CPP_EQ_INT(Z0,cz,"LongInverseRotatePointF Z0");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    TYPE_MAT m; memset(&m,0,sizeof(m));
    m.F.M11=0.5f;m.F.M12=0.3f;m.F.M13=0.1f;
    m.F.M21=0.2f;m.F.M22=0.8f;m.F.M23=0.4f;
    m.F.M31=0.7f;m.F.M32=0.6f;m.F.M33=0.9f;
    for (int i=0;i<10000;i++) {
        S32 x=(S32)rand()-RAND_MAX/2, y=(S32)rand()-RAND_MAX/2, z=(S32)rand()-RAND_MAX/2;
        LongInverseRotatePointF(&m,x,y,z); S32 cx=X0,cy=Y0,cz=Z0;
        call_asm_LongInverseRotatePointF(&m,x,y,z);
        char lbl[96];
        snprintf(lbl,sizeof(lbl),"LIRotF X0 x=%d y=%d z=%d",x,y,z);
        ASSERT_ASM_CPP_EQ_INT(X0,cx,lbl);
        snprintf(lbl,sizeof(lbl),"LIRotF Y0 x=%d y=%d z=%d",x,y,z);
        ASSERT_ASM_CPP_EQ_INT(Y0,cy,lbl);
        snprintf(lbl,sizeof(lbl),"LIRotF Z0 x=%d y=%d z=%d",x,y,z);
        ASSERT_ASM_CPP_EQ_INT(Z0,cz,lbl);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
