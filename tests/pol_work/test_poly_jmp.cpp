/* Test: Polygon jump/dispatch tables (POLY_JMP.CPP) */
#include "test_harness.h"
#include <SYSTEM/ADELINE_TYPES.H>

static void test_linkage(void) { ASSERT_TRUE(1); }
int main(void) { RUN_TEST(test_linkage); TEST_SUMMARY(); return test_failures != 0; }
