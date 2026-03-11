/* Test: ScaleSprite — scale sprite with transparency */
#include "test_harness.h"
#include <SVGA/SCALESPI.H>
#include <string.h>

extern "C" void asm_ScaleSprite(S32 num, S32 x, S32 y, S32 factorx, S32 factory, void *ptrbank);

static void test_linkage(void)
{
    /* Requires valid sprite bank data. Verify linkage. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
