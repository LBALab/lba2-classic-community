/* Test: SinTab/CosTab - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/SINTAB.H>

extern "C" S16 asm_SinTab[];
extern "C" S16 asm_CosTab[];

static void test_equivalence(void)
{
    for (int i = 0; i < 4096; i++)
        ASSERT_ASM_CPP_EQ_INT(asm_SinTab[i], SinTab[i], "SinTab");
    for (int i = 0; i < 4096; i++)
        ASSERT_ASM_CPP_EQ_INT(asm_CosTab[i], CosTab[i], "CosTab");
}

int main(void)
{
    RUN_TEST(test_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
