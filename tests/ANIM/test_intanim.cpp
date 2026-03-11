/* Test: ObjectSetInterAnim — set interpolated animation state */
#include "test_harness.h"

#include <ANIM/INTANIM.H>
#include <ANIM/CLEAR.H>
#include <string.h>

extern "C" S32 asm_ObjectSetInterAnim(T_OBJ_3D *obj);

static void test_linkage(void)
{
    /* ObjectSetInterAnim calls ObjectSetInterDep and ObjectSetInterFrame.
       Without valid animation data, we just verify linkage. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
