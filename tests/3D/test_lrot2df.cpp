/* Test: LongRotate - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROT2D.H>
#include <3D/CAMERA.H>
#include <stdlib.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_LongRotate(S32 x, S32 z, S32 angle);

static void test_equivalence(void)
{
    struct { S32 x,z,angle; } cases[] = {
        {0,0,0},{100,0,0},{100,200,0},{100,0,1024},{100,200,2048},
        {100,200,4096},{-100,-200,512},{500,300,3072},
    };
    for (int i=0;i<(int)(sizeof(cases)/sizeof(cases[0]));i++) {
        LongRotate(cases[i].x,cases[i].z,cases[i].angle);
        S32 cx=X0, cz=Z0;
        asm_LongRotate(cases[i].x,cases[i].z,cases[i].angle);
        ASSERT_ASM_CPP_EQ_INT(X0,cx,"LongRotate X0");
        ASSERT_ASM_CPP_EQ_INT(Z0,cz,"LongRotate Z0");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i=0;i<10000;i++) {
        S32 x=(S32)rand()-RAND_MAX/2, z=(S32)rand()-RAND_MAX/2, a=rand()%4096;
        LongRotate(x,z,a); S32 cx=X0,cz=Z0;
        asm_LongRotate(x,z,a);
        ASSERT_ASM_CPP_EQ_INT(X0,cx,"LongRotate rand X0");
        ASSERT_ASM_CPP_EQ_INT(Z0,cz,"LongRotate rand Z0");
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
