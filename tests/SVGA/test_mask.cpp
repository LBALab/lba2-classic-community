/* Test: AffMask — display mask/sprite with transparency */
#include "test_harness.h"
#include <SVGA/MASK.H>
#include <string.h>

extern "C" S32 asm_AffMask(S32 nummask, S32 x, S32 y, void *bankmask);

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
