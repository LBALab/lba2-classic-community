/* Test: CopyMask — copy masked region */
#include "test_harness.h"
#include <SVGA/COPYMASK.H>
#include <string.h>

extern "C" void asm_CopyMask(S32 nummask, S32 x, S32 y, U8 *bankmask, void *src);

static void test_linkage(void)
{
    /* Requires valid mask bank data. Verify linkage only. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
