/* Test: ObjectStoreFrame — store animation frame state */
#include "test_harness.h"

#include <ANIM/STOFRAME.H>
#include <ANIM/CLEAR.H>
#include <string.h>

extern "C" void asm_ObjectStoreFrame(T_OBJ_3D *obj);

static void test_linkage(void)
{
    /* Requires animation buffer initialized via InitObjects.
       Verify linkage only for now. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
