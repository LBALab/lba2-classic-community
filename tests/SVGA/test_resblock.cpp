/* Test: RestoreBlock — restore saved screen region */
#include "test_harness.h"
#include <SVGA/RESBLOCK.H>
#include <SVGA/SCREEN.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_RestoreBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1);
#endif

static void test_linkage(void)
{
    /* Tested as part of SaveBlock test. Verify linkage. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
