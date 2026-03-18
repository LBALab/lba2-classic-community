/* Test: LongProjectPointIso - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/CAMERA.H>
#include <stdlib.h>

extern "C" void asm_LongProjectPointIso(void);
static void call_asm_LongProjectPointIso(S32 x, S32 y, S32 z) {
    __asm__ __volatile__("call asm_LongProjectPointIso" : : "a"(x), "b"(y), "c"(z) : "memory", "edx");
}

static void test_equivalence(void)
{
    CameraX=0;CameraY=0;CameraZ=0;XCentre=320;YCentre=240;
    struct { S32 x,y,z; } cases[] = {
        {0,0,0},{64,0,0},{0,64,0},{0,0,64},{100,50,200},{-100,-50,-200},
    };
    for (int i=0;i<(int)(sizeof(cases)/sizeof(cases[0]));i++) {
        LongProjectPointIso(cases[i].x,cases[i].y,cases[i].z);
        S32 cxp=Xp,cyp=Yp;
        call_asm_LongProjectPointIso(cases[i].x,cases[i].y,cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(Xp,cxp,"LPPIso Xp");
        ASSERT_ASM_CPP_EQ_INT(Yp,cyp,"LPPIso Yp");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    CameraX=0;CameraY=0;CameraZ=0;XCentre=320;YCentre=240;
    for (int i=0;i<10000;i++) {
        S32 x=(S32)(rand()%2000)-1000, y=(S32)(rand()%2000)-1000, z=(S32)(rand()%2000)-1000;
        LongProjectPointIso(x,y,z); S32 cxp=Xp,cyp=Yp;
        call_asm_LongProjectPointIso(x,y,z);
        ASSERT_ASM_CPP_EQ_INT(Xp,cxp,"LPPIso rand Xp");
        ASSERT_ASM_CPP_EQ_INT(Yp,cyp,"LPPIso rand Yp");
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
