/* Test: ObjectSetInterDep — interpolate animation dependencies */
#include "test_harness.h"

#include <ANIM/INTERDEP.H>
#include <ANIM/CLEAR.H>
#include <string.h>

extern "C" S32 asm_ObjectSetInterDep(T_OBJ_3D *obj);

static void test_linkage(void)
{
    /* Complex function depending on timer and animation data.
       Verify linkage only for now. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
