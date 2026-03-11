/* Test: InitMatrixTransF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/IMATTRA.H>
#include <string.h>
#include <stdlib.h>

extern void InitMatrixTransF(TYPE_MAT *m, S32 tx, S32 ty, S32 tz);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_InitMatrixTransF(void);
static void call_asm_InitMatrixTransF(TYPE_MAT *m, S32 tx, S32 ty, S32 tz) {
    __asm__ __volatile__("call asm_InitMatrixTransF" : : "D"(m), "a"(tx), "b"(ty), "c"(tz) : "memory");
}

static void test_equivalence(void)
{
    struct { S32 tx,ty,tz; } cases[] = {
        {0,0,0}, {100,200,300}, {-500,-1000,-2000}, {2147483647,-2147483647,0},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        TYPE_MAT cm, am;
        memset(&cm,0,sizeof(cm)); memset(&am,0,sizeof(am));
        InitMatrixTransF(&cm, cases[i].tx, cases[i].ty, cases[i].tz);
        call_asm_InitMatrixTransF(&am, cases[i].tx, cases[i].ty, cases[i].tz);
        ASSERT_ASM_CPP_MEM_EQ(&am, &cm, sizeof(TYPE_MAT), "InitMatrixTransF");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        S32 tx=(S32)rand()-RAND_MAX/2, ty=(S32)rand()-RAND_MAX/2, tz=(S32)rand()-RAND_MAX/2;
        TYPE_MAT cm, am;
        memset(&cm,0,sizeof(cm)); memset(&am,0,sizeof(am));
        InitMatrixTransF(&cm, tx, ty, tz);
        call_asm_InitMatrixTransF(&am, tx, ty, tz);
        ASSERT_ASM_CPP_MEM_EQ(&am, &cm, sizeof(TYPE_MAT), "InitMatrixTransF rand");
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
