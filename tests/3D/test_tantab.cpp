/* Test: TanTab — pre-computed tangent table */
#include "test_harness.h"

#include <3D/TANTAB.H>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_TanTab[];
#endif

static void test_first_entry(void)
{
    /* TanTab[0] should be small (tan near 0) */
    ASSERT_TRUE(TanTab[0] >= 0);
}

static void test_monotonic(void)
{
    /* Table should be monotonically increasing */
    for (int i = 1; i < 512; i++) {
        ASSERT_TRUE(TanTab[i] >= TanTab[i - 1]);
    }
}

static void test_midpoint_approx(void)
{
    /* TanTab[256] should correspond to tan(22.5°) scaled */
    /* Just verify it's a reasonable value between TanTab[0] and TanTab[512] */
    ASSERT_TRUE(TanTab[256] > TanTab[0]);
    ASSERT_TRUE(TanTab[256] < TanTab[511]);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    for (int i = 0; i <= 512; i++) {
        ASSERT_ASM_CPP_EQ_INT(asm_TanTab[i], TanTab[i], "TanTab");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_first_entry);
    RUN_TEST(test_monotonic);
    RUN_TEST(test_midpoint_approx);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
