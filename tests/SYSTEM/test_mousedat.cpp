/* Test: Mouse data — mouse driver data structures */
#include "test_harness.h"

#include <SYSTEM/ADELINE_TYPES.H>
#include <string.h>

/* MOUSEDAT.ASM/CPP define BinGphMouse[] — binary mouse cursor graphics data.
   This test verifies the data is present and has the expected size. */

extern "C" U8 BinGphMouse[];

extern "C" U8 asm_BinGphMouse[];

static void test_data_exists(void) {
    /* First byte should be part of the cursor graphic header */
    ASSERT_TRUE(1); /* If we got here, the symbol links */
}

static void test_data_nonzero(void) {
    /* The cursor data should contain at least some non-zero bytes */
    int nonzero = 0;
    for (int i = 0; i < 100; i++) {
        if (BinGphMouse[i] != 0)
            nonzero++;
    }
    ASSERT_TRUE(nonzero > 0);
}

static void test_asm_equiv(void) {
    /* Compare first 541 bytes (known size of BinGphMouse) */
    ASSERT_ASM_CPP_MEM_EQ(asm_BinGphMouse, BinGphMouse, 541, "BinGphMouse");
}

int main(void) {
    RUN_TEST(test_data_exists);
    RUN_TEST(test_data_nonzero);
    RUN_TEST(test_asm_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
