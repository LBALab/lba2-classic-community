/* Test: MySortCompFunc — comparison function for qsort (strcmp-based)
   NOTE: SORT.CPP contains only commented ASM — no C implementation exists.
   This test can only run meaningful tests in ASM mode. */
#include "test_harness.h"

#include <SYSTEM/ADELINE_TYPES.H>
#include <stdlib.h>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" int asm_MySortCompFunc(const void *ptra, const void *ptrb);

static void test_equal_strings(void)
{
    const char *a = "hello";
    const char *b = "hello";
    int result = asm_MySortCompFunc(&a, &b);
    ASSERT_EQ_INT(0, result);
}

static void test_a_less_than_b(void)
{
    const char *a = "alpha";
    const char *b = "beta";
    ASSERT_TRUE(asm_MySortCompFunc(&a, &b) < 0);
}

static void test_a_greater_than_b(void)
{
    const char *a = "beta";
    const char *b = "alpha";
    ASSERT_TRUE(asm_MySortCompFunc(&a, &b) > 0);
}
#endif

static void test_placeholder(void)
{
    /* SORT.CPP has no C implementation — only ASM tests are meaningful */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_placeholder);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_equal_strings);
    RUN_TEST(test_a_less_than_b);
    RUN_TEST(test_a_greater_than_b);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
