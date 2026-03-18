/* Test: Sqr / QSqr - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/SQRROOT.H>
#include <stdlib.h>

/* ASM Sqr uses Watcom register convention: parm [eax], result in eax */
extern "C" void asm_Sqr(void);
static U32 call_asm_Sqr(U32 x) {
    U32 result;
    __asm__ __volatile__(
        "call asm_Sqr"
        : "=a"(result)
        : "a"(x)
        : "memory", "ebx", "ecx", "edx", "esi", "edi");
    return result;
}

/* ASM QSqr uses Watcom register convention: parm [eax] [edx], result in eax */
extern "C" void asm_QSqr(void);
static U32 call_asm_QSqr(U32 xLow, U32 xHigh) {
    U32 result;
    __asm__ __volatile__(
        "call asm_QSqr"
        : "=a"(result)
        : "a"(xLow), "d"(xHigh)
        : "memory", "ebx", "ecx", "esi", "edi");
    return result;
}

static void test_equivalence(void)
{
    U32 sqr_cases[] = {0, 1, 2, 3, 4, 9, 100, 10000, 1000000, 0xFFFFFFFF};
    for (int i = 0; i < (int)(sizeof(sqr_cases)/sizeof(sqr_cases[0])); i++) {
        ASSERT_ASM_CPP_EQ_INT(call_asm_Sqr(sqr_cases[i]), Sqr(sqr_cases[i]), "Sqr");
    }
    struct { U32 lo, hi; } qsqr_cases[] = {
        {0,0}, {1,0}, {100,0}, {0,1}, {0,2}, {0xFFFFFFFF,0},
    };
    for (int i = 0; i < (int)(sizeof(qsqr_cases)/sizeof(qsqr_cases[0])); i++) {
        U32 cpp = QSqr(qsqr_cases[i].lo, qsqr_cases[i].hi);
        U32 asr = call_asm_QSqr(qsqr_cases[i].lo, qsqr_cases[i].hi);
        ASSERT_ASM_CPP_EQ_INT(asr, cpp, "QSqr");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        U32 x = ((U32)rand() << 16) | (U32)rand();
        ASSERT_ASM_CPP_EQ_INT(call_asm_Sqr(x), Sqr(x), "Sqr rand");
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
