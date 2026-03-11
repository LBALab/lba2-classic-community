/* Test: ObjectSetInterAnim — combined interpolation (calls InterDep + InterFrame).
 *
 * NOTE: ASM version inherits the INTERDEP function pointer issue.
 * Tests are CPP-only; ASM-vs-CPP comparison is skipped.
 * Marked [~] in ASM_VALIDATION_PROGRESS.md.
 */
#include "test_harness.h"
#include <ANIM/INTANIM.H>
#include <ANIM/ANIM.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <ANIM.H>
#include <OBJECT/AFF_OBJ.H>
#include "anim_test_fixture.h"
#include <string.h>

extern "C" S32 asm_ObjectSetInterAnim(T_OBJ_3D *obj);
extern "C" U32 TimerRefHR;

static U8 test_anim_data[512];

static void setup(T_OBJ_3D *obj)
{
    build_anim_header(test_anim_data, 2, 3, 0, 100);
    set_anim_group(test_anim_data, 3, 0, 0, 0, 0, 0, 0);
    set_anim_group(test_anim_data, 3, 0, 1, 0, 100, 200, 300);
    set_anim_group(test_anim_data, 3, 0, 2, 0, 400, 500, 600);
    set_anim_group(test_anim_data, 3, 1, 0, 0, 0, 0, 0);
    set_anim_group(test_anim_data, 3, 1, 1, 0, 200, 400, 600);
    set_anim_group(test_anim_data, 3, 1, 2, 0, 800, 1000, 1200);

    init_test_obj(obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    ObjectInitAnim(obj, test_anim_data);
    obj->Status = FLAG_CHANGE;
    obj->LastFrame = 0;
    obj->NextFrame = 1;
    obj->LastOfsFrame = (PTR_U32)anim_frame_offset(3, 0);
    obj->NextOfsFrame = (PTR_U32)anim_frame_offset(3, 1);
    obj->LastOfsIsPtr = 0;
    obj->LastTimer = 0;
    obj->NextTimer = 100;
}

static void test_cpp_interp_midpoint(void)
{
    T_OBJ_3D obj;
    setup(&obj);
    TimerRefHR = 50;  /* 50% */
    S32 result = ObjectSetInterAnim(&obj);
    /* After InterDep sets Interpolator and InterFrame interpolates,
     * CurrentFrame should be ~midpoint of frame 0 and frame 1 values */
    ASSERT_TRUE(obj.CurrentFrame[0].Alpha >= 130 && obj.CurrentFrame[0].Alpha <= 170);
    (void)result;
}

static void test_cpp_frame_transition(void)
{
    T_OBJ_3D obj;
    setup(&obj);
    TimerRefHR = 150;  /* Past NextTimer, triggers frame advance */
    S32 result = ObjectSetInterAnim(&obj);
    ASSERT_TRUE(obj.Status & FLAG_FRAME);
    (void)result;
}

static void test_cpp_no_change(void)
{
    T_OBJ_3D obj;
    setup(&obj);
    obj.Status = 0;  /* No FLAG_CHANGE — should return immediately */
    S16 saved = obj.CurrentFrame[0].Alpha;
    TimerRefHR = 50;
    S32 result = ObjectSetInterAnim(&obj);
    ASSERT_EQ_INT(saved, obj.CurrentFrame[0].Alpha);
    (void)result;
}

int main(void)
{
    RUN_TEST(test_cpp_interp_midpoint);
    RUN_TEST(test_cpp_frame_transition);
    RUN_TEST(test_cpp_no_change);
    TEST_SUMMARY();
    return test_failures != 0;
}
