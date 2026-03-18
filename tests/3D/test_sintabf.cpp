/* Test: SinTabF/CosTabF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/SINTABF.H>
#include <string.h>

extern "C" float asm_SinTabF[];
extern "C" float asm_CosTabF[];

static void test_equivalence(void)
{
    ASSERT_ASM_CPP_MEM_EQ(asm_SinTabF, SinTabF, 4096 * sizeof(float), "SinTabF");
    ASSERT_ASM_CPP_MEM_EQ(asm_CosTabF, CosTabF, 4096 * sizeof(float), "CosTabF");
}

int main(void)
{
    RUN_TEST(test_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
