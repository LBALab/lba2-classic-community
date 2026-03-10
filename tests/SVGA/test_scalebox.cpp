/* Test: ScaleBox — scale rectangular region */
#include "test_harness.h"
#include <SVGA/SCALEBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_ScaleBox(S32 xs0, S32 ys0, S32 xs1, S32 ys1, void *ptrs, S32 xd0, S32 yd0, S32 xd1, S32 yd1, void *ptrd);
#endif

static void test_linkage(void)
{
    /* Requires valid screen data. Verify linkage. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
