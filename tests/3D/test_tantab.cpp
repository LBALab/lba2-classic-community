/* Test: TanTab - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/TANTAB.H>

extern "C" S32 asm_TanTab[];

static void test_equivalence(void) {
    for (int i = 0; i <= 512; i++)
        ASSERT_ASM_CPP_EQ_INT(asm_TanTab[i], TanTab[i], "TanTab");
}

int main(void) {
    RUN_TEST(test_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
