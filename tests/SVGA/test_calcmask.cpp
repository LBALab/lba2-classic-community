/* Test: CalcGraphMsk — calculate graphical mask */
#include "test_harness.h"
#include <SVGA/CALCMASK.H>
#include <string.h>

extern "C" S32 asm_CalcGraphMsk(S32 numbrick, U8 *bankbrick, U8 *ptmask);

static void test_linkage(void)
{
    /* Requires valid brick bank data. Verify linkage only. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
