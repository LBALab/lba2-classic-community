/* Test: SetProjection / SetIsoProjection - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <3D/LPROJ.H>
#include <stdlib.h>

extern "C" void asm_SetProjection(S32 xc, S32 yc, S32 clip, S32 fx, S32 fy);
extern "C" void asm_SetIsoProjection(S32 xc, S32 yc);

static void test_equivalence(void)
{
    struct { S32 xc,yc,cl,fx,fy; } cases[] = {
        {320,240,10,500,400},{0,0,1,100,100},{640,480,50,1000,800},
    };
    for (int i=0;i<(int)(sizeof(cases)/sizeof(cases[0]));i++) {
        asm_SetProjection(cases[i].xc,cases[i].yc,cases[i].cl,cases[i].fx,cases[i].fy);
        S32 axc=XCentre,ayc=YCentre,anc=NearClip,afx=LFactorX,afy=LFactorY,atp=TypeProj;
        SetProjection(cases[i].xc,cases[i].yc,cases[i].cl,cases[i].fx,cases[i].fy);
        ASSERT_ASM_CPP_EQ_INT(axc,XCentre,"SetProjection XCentre");
        ASSERT_ASM_CPP_EQ_INT(ayc,YCentre,"SetProjection YCentre");
        ASSERT_ASM_CPP_EQ_INT(anc,NearClip,"SetProjection NearClip");
        ASSERT_ASM_CPP_EQ_INT(afx,LFactorX,"SetProjection LFactorX");
        ASSERT_ASM_CPP_EQ_INT(afy,LFactorY,"SetProjection LFactorY");
        ASSERT_ASM_CPP_EQ_INT(atp,TypeProj,"SetProjection TypeProj");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i=0;i<10000;i++) {
        S32 xc=rand()%1280, yc=rand()%960, cl=rand()%100+1, fx=rand()%2000+1, fy=rand()%2000+1;
        asm_SetProjection(xc,yc,cl,fx,fy);
        S32 axc=XCentre,ayc=YCentre,anc=NearClip,afx=LFactorX,afy=LFactorY;
        SetProjection(xc,yc,cl,fx,fy);
        ASSERT_ASM_CPP_EQ_INT(axc,XCentre,"SetProjection rand XCentre");
        ASSERT_ASM_CPP_EQ_INT(ayc,YCentre,"SetProjection rand YCentre");
        ASSERT_ASM_CPP_EQ_INT(anc,NearClip,"SetProjection rand NearClip");
        ASSERT_ASM_CPP_EQ_INT(afx,LFactorX,"SetProjection rand LFactorX");
        ASSERT_ASM_CPP_EQ_INT(afy,LFactorY,"SetProjection rand LFactorY");
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
