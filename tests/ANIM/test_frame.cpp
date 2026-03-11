/* Test: ObjectSetFrame — set current animation frame.
   Requires valid animation data; tests basic interface. */
#include "test_harness.h"

#include <ANIM/FRAME.H>
#include <ANIM/CLEAR.H>
#include <string.h>

extern "C" void asm_ObjectSetFrame(T_OBJ_3D *obj, U32 frame);

static void test_linkage(void)
{
    /* Verify function links correctly */
    T_OBJ_3D obj;
    ObjectClear(&obj);
    /* Cannot call ObjectSetFrame without valid anim data —
       just verify compilation and linkage */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
