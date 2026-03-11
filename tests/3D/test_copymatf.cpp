/* Test: CopyMatrixF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/COPYMAT.H>
#include <string.h>
#include <stdlib.h>

extern void CopyMatrixF(TYPE_MAT *d, TYPE_MAT *s);

#ifdef LBA2_ASM_TESTS
extern "C" void asm_CopyMatrixF(void);
static void call_asm_CopyMatrixF(TYPE_MAT *d, TYPE_MAT *s) {
    __asm__ __volatile__("call asm_CopyMatrixF" : : "D"(d), "S"(s) : "memory");
}

static void test_equivalence(void)
{
    TYPE_MAT src; float *sf = &src.F.M11;
    sf[0]=1.1f; sf[1]=2.2f; sf[2]=3.3f; sf[3]=4.4f; sf[4]=5.5f; sf[5]=6.6f;
    sf[6]=7.7f; sf[7]=8.8f; sf[8]=9.9f; sf[9]=10.f; sf[10]=11.f; sf[11]=12.f;
    TYPE_MAT dc, da;
    memset(&dc, 0, sizeof(dc)); CopyMatrixF(&dc, &src);
    memset(&da, 0, sizeof(da)); call_asm_CopyMatrixF(&da, &src);
    ASSERT_ASM_CPP_MEM_EQ(&da, &dc, sizeof(TYPE_MAT), "CopyMatrixF");
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        TYPE_MAT src; float *sf = &src.F.M11;
        for (int j = 0; j < 12; j++) sf[j] = (float)((S32)rand()-RAND_MAX/2)/100.f;
        TYPE_MAT dc, da;
        memset(&dc, 0xCC, sizeof(dc)); CopyMatrixF(&dc, &src);
        memset(&da, 0xCC, sizeof(da)); call_asm_CopyMatrixF(&da, &src);
        ASSERT_ASM_CPP_MEM_EQ(&da, &dc, sizeof(TYPE_MAT), "CopyMatrixF rand");
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
