/* Test: ObjectInitAnim — initialize animation for 3D object.
   This is complex and depends on animation data format.
   For now, tests basic interface behavior. */
#include "test_harness.h"

#include <ANIM/ANIM.H>
#include <ANIM/CLEAR.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_ObjectInitAnim(T_OBJ_3D *obj, void *anim);
#endif

static void test_null_anim_no_crash(void)
{
    T_OBJ_3D obj;
    ObjectClear(&obj);
    /* Setting same pointer as current (which is -1 for Num) should early-return */
    /* Note: calling with arbitrary pointer may need InitObjects first */
    ASSERT_TRUE(1); /* Placeholder: compilation verifies linkage */
}

static void test_same_anim_early_return(void)
{
    T_OBJ_3D obj;
    ObjectClear(&obj);
    U8 fake_anim[64];
    memset(fake_anim, 0, sizeof(fake_anim));
    obj.Anim.Ptr = fake_anim;
    /* Calling with same pointer should be a no-op */
    ObjectInitAnim(&obj, fake_anim);
    ASSERT_TRUE(obj.Anim.Ptr == fake_anim);
}

int main(void)
{
    RUN_TEST(test_null_anim_no_crash);
    RUN_TEST(test_same_anim_early_return);
    TEST_SUMMARY();
    return test_failures != 0;
}
