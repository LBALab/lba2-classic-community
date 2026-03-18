/* Test: ObjectSetInterFrame — interpolate between last and next animation frames */
#include "test_harness.h"
#include <ANIM/INTFRAME.H>
#include <ANIM/ANIM.H>
#include <ANIM/FRAME.H>
#include <ANIM/STOFRAME.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <ANIM.H>
#include <OBJECT/AFF_OBJ.H>
#include "anim_test_fixture.h"
#include <string.h>

extern "C" void asm_ObjectSetInterFrame(T_OBJ_3D *obj);

static U8 test_anim_data[512];

/* Build 2-frame, 3-group anim with known interpolation targets.
 * Frame 0 group 1: rotate(100, 200, 300)
 * Frame 1 group 1: rotate(200, 400, 600)
 * At 50% interpolation: (150, 300, 450) */
static void build_interp_anim(void)
{
    build_anim_header(test_anim_data, 2, 3, 0, 100);
    set_anim_group(test_anim_data, 3, 0, 1, 0, 100, 200, 300);
    set_anim_group(test_anim_data, 3, 0, 2, 0, 0, 0, 0);
    set_anim_group(test_anim_data, 3, 1, 1, 0, 200, 400, 600);
    set_anim_group(test_anim_data, 3, 1, 2, 0, 0, 0, 0);
}

static void setup_interp(T_OBJ_3D *obj)
{
    build_interp_anim();
    init_test_obj(obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    ObjectInitAnim(obj, test_anim_data);
    /* Set up for interpolation: Status needs FLAG_CHANGE but not FLAG_FRAME */
    obj->Status = FLAG_CHANGE;
    /* LastOfsFrame = frame 0 offset, NextOfsFrame = frame 1 offset */
    obj->LastOfsFrame = (PTR_U32)anim_frame_offset(3, 0);
    obj->NextOfsFrame = (PTR_U32)anim_frame_offset(3, 1);
    obj->LastOfsIsPtr = 0;
    obj->Interpolator = 0x8000;  /* 50% = 0x8000 out of 0x10000 */
}

static void test_cpp_interp_50pct(void)
{
    T_OBJ_3D obj;
    setup_interp(&obj);
    ObjectSetInterFrame(&obj);
    /* At 50%: Alpha = 100 + (200-100)*0x8000/0x10000 = 100 + 50 = 150 */
    ASSERT_EQ_INT(150, obj.CurrentFrame[0].Alpha);
    ASSERT_EQ_INT(300, obj.CurrentFrame[0].Beta);
    ASSERT_EQ_INT(450, obj.CurrentFrame[0].Gamma);
}

static void test_cpp_interp_0pct(void)
{
    T_OBJ_3D obj;
    setup_interp(&obj);
    obj.Interpolator = 0;  /* 0% = fully "last" frame */
    ObjectSetInterFrame(&obj);
    ASSERT_EQ_INT(100, obj.CurrentFrame[0].Alpha);
    ASSERT_EQ_INT(200, obj.CurrentFrame[0].Beta);
    ASSERT_EQ_INT(300, obj.CurrentFrame[0].Gamma);
}

static void test_cpp_no_interp_when_flag_frame(void)
{
    T_OBJ_3D obj;
    setup_interp(&obj);
    obj.Status = FLAG_CHANGE | FLAG_FRAME;  /* FLAG_FRAME blocks interpolation */
    S16 saved_alpha = obj.CurrentFrame[0].Alpha;
    ObjectSetInterFrame(&obj);
    /* CurrentFrame should be unchanged */
    ASSERT_EQ_INT(saved_alpha, obj.CurrentFrame[0].Alpha);
}

static void test_asm_equiv(void)
{
    T_OBJ_3D cpp_obj, asm_obj;
    TransFctAnim = NULL;

    setup_interp(&cpp_obj);
    ObjectSetInterFrame(&cpp_obj);

    setup_interp(&asm_obj);
    asm_ObjectSetInterFrame(&asm_obj);

    ASSERT_ASM_CPP_MEM_EQ(asm_obj.CurrentFrame, cpp_obj.CurrentFrame,
                          2 * sizeof(T_GROUP_INFO), "ObjectSetInterFrame 50%");
}

static void test_asm_equiv_25pct(void)
{
    T_OBJ_3D cpp_obj, asm_obj;
    TransFctAnim = NULL;

    setup_interp(&cpp_obj);
    cpp_obj.Interpolator = 0x4000;  /* 25% */
    ObjectSetInterFrame(&cpp_obj);

    setup_interp(&asm_obj);
    asm_obj.Interpolator = 0x4000;
    asm_ObjectSetInterFrame(&asm_obj);

    ASSERT_ASM_CPP_MEM_EQ(asm_obj.CurrentFrame, cpp_obj.CurrentFrame,
                          2 * sizeof(T_GROUP_INFO), "ObjectSetInterFrame 25%");
}

int main(void)
{
    RUN_TEST(test_cpp_interp_50pct);
    RUN_TEST(test_cpp_interp_0pct);
    RUN_TEST(test_cpp_no_interp_when_flag_frame);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_asm_equiv_25pct);
    TEST_SUMMARY();
    return test_failures != 0;
}
