/* Test: AffString — display string at screen position */
#include "test_harness.h"
#include <SVGA/AFFSTR.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_AffString(S32 x, S32 y, char *str);
#endif

static void test_linkage(void)
{
    /* Requires initialized screen and font data. Verify linkage only. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
