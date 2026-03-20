/* Test: Mouse data — mouse driver data structures */
#include "test_harness.h"

#include <SYSTEM/ADELINE_TYPES.H>
#include <string.h>

/* MOUSEDAT.ASM/CPP define BinGphMouse[] — binary mouse cursor graphics data.
   This test verifies the data is present and has the expected size. */

extern "C" U8 BinGphMouse[];

extern "C" U8 asm_BinGphMouse[];

static void test_fixed_bytes(void) {
    ASSERT_EQ_INT(0x14, BinGphMouse[0]);
    ASSERT_EQ_INT(0x8d, BinGphMouse[4]);
    ASSERT_EQ_INT(0x06, BinGphMouse[8]);
    ASSERT_EQ_INT(0xba, BinGphMouse[12]);
    ASSERT_EQ_INT(0x7c, BinGphMouse[16]);
    ASSERT_EQ_INT(0x0a, BinGphMouse[20]);
}

static void test_asm_equiv(void) {
    /* Compare first 541 bytes (known size of BinGphMouse) */
    ASSERT_ASM_CPP_MEM_EQ(asm_BinGphMouse, BinGphMouse, 541, "BinGphMouse");
}

int main(void) {
    RUN_TEST(test_fixed_bytes);
    RUN_TEST(test_asm_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
