/* Test: SinTabF/CosTabF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/SINTABF.H>

extern "C" float asm_SinTabF[];
extern "C" float asm_CosTabF[];

static void test_equivalence(void)
{
    for (int i = 0; i < 4096; i++)
        ASSERT_ASM_CPP_NEAR_F(asm_SinTabF[i], SinTabF[i], 0.0001f, "SinTabF");
    for (int i = 0; i < 4096; i++)
        ASSERT_ASM_CPP_NEAR_F(asm_CosTabF[i], CosTabF[i], 0.0001f, "CosTabF");
}

int main(void)
{
    RUN_TEST(test_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
