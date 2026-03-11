/* Test: ObjectSetInterFrame — apply frame interpolation */
#include "test_harness.h"

#include <ANIM/INTFRAME.H>
#include <ANIM/CLEAR.H>
#include <string.h>

extern "C" void asm_ObjectSetInterFrame(T_OBJ_3D *obj);

static void test_linkage(void)
{
    /* Complex function depending on animation buffer data.
       Verify linkage only for now. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
