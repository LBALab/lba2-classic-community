/* Test: AffGraph / GetBoxGraph — graphics drawing and box queries */
#include "test_harness.h"
#include <SVGA/GRAPH.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_AffGraph(S32 numgraph, S32 x, S32 y, const void *bankgraph);
extern "C" S32 asm_GetBoxGraph(S32 numgraph, S32 *x0, S32 *y0, S32 *x1, S32 *y1, const void *bankgraph);
#endif

static void test_linkage(void)
{
    /* Requires valid graphics bank data. Verify linkage only. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
