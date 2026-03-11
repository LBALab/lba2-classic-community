/* Test: TanTab - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/TANTAB.H>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_TanTab[];

static void test_equivalence(void)
{
    for (int i = 0; i <= 512; i++)
        ASSERT_ASM_CPP_EQ_INT(asm_TanTab[i], TanTab[i], "TanTab");
}
#endif

int main(void)
{
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_equivalence);
#else
    printf("SKIPPED - ASM tests not enabled\n");
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
