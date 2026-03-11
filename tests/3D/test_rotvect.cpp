/* Test: RotateVector - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROTVECT.H>
#include <3D/CAMERA.H>
#include <stdlib.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_RotateVector(S32 n, S32 a, S32 b, S32 g);

static void test_equivalence(void)
{
    struct { S32 n,a,b,g; } cases[] = {
        {0,0,0,0},{100,0,0,0},{100,1024,0,0},{100,0,1024,0},{100,0,0,1024},
        {500,512,512,512},{-100,2048,1024,3072},
    };
    for (int i=0;i<(int)(sizeof(cases)/sizeof(cases[0]));i++) {
        RotateVector(cases[i].n,cases[i].a,cases[i].b,cases[i].g);
        S32 cx=X0,cy=Y0,cz=Z0;
        asm_RotateVector(cases[i].n,cases[i].a,cases[i].b,cases[i].g);
        ASSERT_ASM_CPP_EQ_INT(X0,cx,"RotateVector X0");
        ASSERT_ASM_CPP_EQ_INT(Y0,cy,"RotateVector Y0");
        ASSERT_ASM_CPP_EQ_INT(Z0,cz,"RotateVector Z0");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i=0;i<10000;i++) {
        S32 n=(S32)rand()-RAND_MAX/2, a=rand()%4096, b=rand()%4096, g=rand()%4096;
        RotateVector(n,a,b,g); S32 cx=X0,cy=Y0,cz=Z0;
        asm_RotateVector(n,a,b,g);
        ASSERT_ASM_CPP_EQ_INT(X0,cx,"RotateVector rand X0");
        ASSERT_ASM_CPP_EQ_INT(Y0,cy,"RotateVector rand Y0");
        ASSERT_ASM_CPP_EQ_INT(Z0,cz,"RotateVector rand Z0");
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
