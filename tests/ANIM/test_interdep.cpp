/* Test: ObjectSetInterDep — interpolate animation dependencies.
 *
 * NOTE: ASM version calls InitMatrixStd and RotatePoint via DWORD function
 * pointers (EXTRN C InitMatrixStd: DWORD). These will crash because the
 * function symbols resolve to the function body, not to pointer variables.
 * Tests are CPP-only; ASM-vs-CPP comparison is skipped.
 * Marked [~] in ASM_VALIDATION_PROGRESS.md.
 */
#include "test_harness.h"
#include <ANIM/INTERDEP.H>
#include <ANIM/ANIM.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <ANIM.H>
#include <OBJECT/AFF_OBJ.H>
#include "anim_test_fixture.h"
#include <string.h>

extern "C" S32 asm_ObjectSetInterDep(T_OBJ_3D *obj);
extern "C" U32 TimerRefHR;

static U8 test_anim_data[512];

static void build_dep_anim(void)
{
    build_anim_header(test_anim_data, 3, 3, 1, 100);
    /* Master = 0 (no rotation) for simplicity */
    set_anim_group(test_anim_data, 3, 0, 0, 0, 0, 0, 0); /* master group frame 0 */
    set_anim_group(test_anim_data, 3, 0, 1, 0, 100, 200, 300);
    set_anim_group(test_anim_data, 3, 0, 2, 0, 400, 500, 600);
    set_anim_group(test_anim_data, 3, 1, 0, 0, 0, 0, 0);
    set_anim_group(test_anim_data, 3, 1, 1, 0, 200, 400, 600);
    set_anim_group(test_anim_data, 3, 1, 2, 0, 800, 1000, 1200);
    set_anim_group(test_anim_data, 3, 2, 0, 0, 0, 0, 0);
    set_anim_group(test_anim_data, 3, 2, 1, 0, 300, 600, 900);
    set_anim_group(test_anim_data, 3, 2, 2, 0, 1200, 1500, 1800);
}

static void setup_dep(T_OBJ_3D *obj)
{
    build_dep_anim();
    init_test_obj(obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    ObjectInitAnim(obj, test_anim_data);
    /* Set Status to FLAG_CHANGE to enable interpolation */
    obj->Status = FLAG_CHANGE;
    obj->LastFrame = 0;
    obj->NextFrame = 1;
    obj->LastOfsFrame = (PTR_U32)anim_frame_offset(3, 0);
    obj->NextOfsFrame = (PTR_U32)anim_frame_offset(3, 1);
    obj->LastOfsIsPtr = 0;
    obj->LastTimer = 0;
    obj->NextTimer = 100;
}

static void test_cpp_midpoint(void)
{
    T_OBJ_3D obj;
    setup_dep(&obj);
    TimerRefHR = 50;  /* 50% between LastTimer=0 and NextTimer=100 */
    S32 result = ObjectSetInterDep(&obj);
    /* Interpolator should be ~0x8000 (50%) */
    ASSERT_TRUE(obj.Interpolator > 0x7000 && obj.Interpolator < 0x9000);
    (void)result;
}

static void test_cpp_frame_advance(void)
{
    T_OBJ_3D obj;
    setup_dep(&obj);
    TimerRefHR = 150;  /* Past NextTimer=100, should advance frame */
    S32 result = ObjectSetInterDep(&obj);
    /* Status should include FLAG_FRAME (frame transition) */
    ASSERT_TRUE(obj.Status & FLAG_FRAME);
    (void)result;
}

static void test_cpp_loop(void)
{
    T_OBJ_3D obj;
    setup_dep(&obj);
    /* Advance time past NextTimer to trigger frame advance */
    TimerRefHR = 200;
    S32 result = ObjectSetInterDep(&obj);
    /* After frame advance, NextFrame should have changed from initial value */
    ASSERT_TRUE(obj.NextFrame != 1 || (obj.Status & FLAG_FRAME));
    (void)result;
}

int main(void)
{
    RUN_TEST(test_cpp_midpoint);
    RUN_TEST(test_cpp_frame_advance);
    RUN_TEST(test_cpp_loop);
    TEST_SUMMARY();
    return test_failures != 0;
}
