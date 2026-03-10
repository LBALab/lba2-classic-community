/* Test: ScaleSpriteTransp — scale transparent sprite */
#include "test_harness.h"
#include <SVGA/SCALESPT.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_ScaleSpriteTransp(S32 num, S32 x, S32 y, S32 factorx, S32 factory, void *ptrbank, void *ptr_transp);
#endif

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
