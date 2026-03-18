/* Test: LongProjectPoint3D - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <stdlib.h>

extern S32 LongProjectPoint3D(S32 x, S32 y, S32 z);
extern "C" void asm_LongProjectPoint3DF(void);
static S32 call_asm_LongProjectPoint3DF(S32 x, S32 y, S32 z) {
    S32 result;
    __asm__ __volatile__("call asm_LongProjectPoint3DF"
        : "=a"(result) : "0"(x), "b"(y), "c"(z) : "memory", "edx");
    return result;
}

static void test_equivalence(void)
{
    SetProjection(320,240,1,300,300);
    CameraXr=0;CameraYr=0;CameraZr=1000;CameraZrClip=CameraZr-NearClip;
    struct { S32 x,y,z; } cases[] = {
        {0,0,0},{100,0,0},{0,100,0},{-100,-100,0},{0,0,500},{0,0,2000},
    };
    for (int i=0;i<(int)(sizeof(cases)/sizeof(cases[0]));i++) {
        S32 cr=LongProjectPoint3D(cases[i].x,cases[i].y,cases[i].z);
        S32 cxp=Xp,cyp=Yp;
        S32 ar=call_asm_LongProjectPoint3DF(cases[i].x,cases[i].y,cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(ar,cr,"LongProjectPoint3D ret");
        if(cr==1){ASSERT_ASM_CPP_EQ_INT(Xp,cxp,"LPP3D Xp");ASSERT_ASM_CPP_EQ_INT(Yp,cyp,"LPP3D Yp");}
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    SetProjection(320,240,1,300,300);
    CameraXr=0;CameraYr=0;CameraZr=1000;CameraZrClip=CameraZr-NearClip;
    for (int i=0;i<10000;i++) {
        S32 x=(S32)(rand()%2000)-1000, y=(S32)(rand()%2000)-1000, z=(S32)(rand()%2000)-1000;
        S32 cr=LongProjectPoint3D(x,y,z); S32 cxp=Xp,cyp=Yp;
        S32 ar=call_asm_LongProjectPoint3DF(x,y,z);
        ASSERT_ASM_CPP_EQ_INT(ar,cr,"LPP3D rand ret");
        if(cr==1){ASSERT_ASM_CPP_EQ_INT(Xp,cxp,"LPP3D rand Xp");ASSERT_ASM_CPP_EQ_INT(Yp,cyp,"LPP3D rand Yp");}
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
