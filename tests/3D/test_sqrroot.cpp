/* Test: Sqr / QSqr - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/SQRROOT.H>
#include <stdlib.h>

#ifdef LBA2_ASM_TESTS
extern "C" U32 asm_Sqr(U32 x);
extern "C" U32 asm_QSqr(U32 xLow, U32 xHigh);

static void test_equivalence(void)
{
    U32 sqr_cases[] = {0, 1, 2, 3, 4, 9, 100, 10000, 1000000, 0xFFFFFFFF};
    for (int i = 0; i < (int)(sizeof(sqr_cases)/sizeof(sqr_cases[0])); i++) {
        ASSERT_ASM_CPP_EQ_INT(asm_Sqr(sqr_cases[i]), Sqr(sqr_cases[i]), "Sqr");
    }
    struct { U32 lo, hi; } qsqr_cases[] = {
        {0,0}, {1,0}, {100,0}, {0,1}, {0,2}, {0xFFFFFFFF,0},
    };
    for (int i = 0; i < (int)(sizeof(qsqr_cases)/sizeof(qsqr_cases[0])); i++) {
        U32 cpp = QSqr(qsqr_cases[i].lo, qsqr_cases[i].hi);
        U32 asr = asm_QSqr(qsqr_cases[i].lo, qsqr_cases[i].hi);
        S32 diff = (S32)asr - (S32)cpp;
        if (diff < -1 || diff > 1)
            FAIL_MSG("[QSqr] ASM=%u vs CPP=%u (diff %d)", asr, cpp, diff);
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        U32 x = ((U32)rand() << 16) | (U32)rand();
        ASSERT_ASM_CPP_EQ_INT(asm_Sqr(x), Sqr(x), "Sqr rand");
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
