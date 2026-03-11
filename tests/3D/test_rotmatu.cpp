/* Test: RotateMatrixU - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROTMAT.H>
#include <3D/IMATSTD.H>
#include <string.h>
#include <stdlib.h>

extern void RotateMatrixU(TYPE_MAT *d, TYPE_MAT *s, S32 x, S32 y, S32 z);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_RotateMatrixU(TYPE_MAT *d, TYPE_MAT *s, S32 x, S32 y, S32 z);

static void test_equivalence(void)
{
    TYPE_MAT src; memset(&src,0,sizeof(src));
    src.F.M11=1;src.F.M22=1;src.F.M33=1;
    struct { S32 a,b,g; } cases[] = {
        {0,0,0},{1024,0,0},{0,1024,0},{0,0,1024},{512,512,512},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        TYPE_MAT cm,am;
        RotateMatrixU(&cm,&src,cases[i].a,cases[i].b,cases[i].g);
        asm_RotateMatrixU(&am,&src,cases[i].a,cases[i].b,cases[i].g);
        for (int j=0;j<9;j++)
            ASSERT_ASM_CPP_NEAR_F((&am.F.M11)[j],(&cm.F.M11)[j],0.01f,"RotateMatrixU");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    TYPE_MAT src; memset(&src,0,sizeof(src));
    src.F.M11=1;src.F.M22=1;src.F.M33=1;
    for (int i = 0; i < 10000; i++) {
        S32 a=rand()%4096, b=rand()%4096, g=rand()%4096;
        TYPE_MAT cm,am;
        RotateMatrixU(&cm,&src,a,b,g);
        asm_RotateMatrixU(&am,&src,a,b,g);
        for (int j=0;j<9;j++)
            ASSERT_ASM_CPP_NEAR_F((&am.F.M11)[j],(&cm.F.M11)[j],0.01f,"RotateMatrixU rand");
    }
}
#endif

int main(void)
{
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
#else
    printf("SKIPPED - ASM tests not enabled\n");
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
