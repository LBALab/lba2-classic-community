/* Test: InitMatrixStdF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/IMATSTD.H>
#include <3D/SINTABF.H>
#include <string.h>
#include <stdlib.h>

extern void InitMatrixStdF(TYPE_MAT *m, S32 a, S32 b, S32 g);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_InitMatrixStdF(void);
static void call_asm_InitMatrixStdF(TYPE_MAT *m, S32 a, S32 b, S32 g) {
    __asm__ __volatile__("call asm_InitMatrixStdF"
        : "+a"(a), "+b"(b), "+c"(g) : "D"(m) : "memory", "edx", "esi");
}

static void test_equivalence(void)
{
    struct { S32 a, b, g; } cases[] = {
        {0,0,0}, {1024,0,0}, {0,1024,0}, {0,0,1024},
        {512,512,512}, {2048,2048,2048}, {4096,4096,4096}, {-1024,500,3000},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        TYPE_MAT cm, am;
        InitMatrixStdF(&cm, cases[i].a, cases[i].b, cases[i].g);
        call_asm_InitMatrixStdF(&am, cases[i].a, cases[i].b, cases[i].g);
        ASSERT_ASM_CPP_MEM_EQ(&am, &cm, sizeof(TYPE_MAT), "InitMatrixStdF");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        S32 a = rand()%4096, b = rand()%4096, g = rand()%4096;
        TYPE_MAT cm, am;
        InitMatrixStdF(&cm, a, b, g);
        call_asm_InitMatrixStdF(&am, a, b, g);
        ASSERT_ASM_CPP_MEM_EQ(&am, &cm, sizeof(TYPE_MAT), "InitMatrixStdF rand");
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
