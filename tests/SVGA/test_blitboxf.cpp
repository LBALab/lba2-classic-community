/* Test: BlitBoxF — fast blit rectangular region.
   NOTE: BLITBOXF.CPP contains only commented ASM — no C implementation. */
#include "test_harness.h"
#include <SYSTEM/ADELINE_TYPES.H>

static void test_placeholder(void)
{
    /* No C implementation exists. ASM-only test. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_placeholder);
    TEST_SUMMARY();
    return test_failures != 0;
}
