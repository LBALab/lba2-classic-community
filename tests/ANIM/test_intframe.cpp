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
 * Group 1 exercises rotation interpolation.
 * Group 2 exercises translation interpolation.
 */
static void build_interp_anim(void) {
    build_anim_header(test_anim_data, 2, 3, 0, 100);
    set_anim_group(test_anim_data, 3, 0, 1, TYPE_ROTATE, 100, 200, 300);
    set_anim_group(test_anim_data, 3, 0, 2, TYPE_TRANSLATE, 1000, -2000, 3000);
    set_anim_group(test_anim_data, 3, 1, 1, TYPE_ROTATE, 200, 400, 600);
    set_anim_group(test_anim_data, 3, 1, 2, TYPE_TRANSLATE, -1000, 2000, -3000);
}

static void assert_group_eq(const T_GROUP_INFO *group,
                            S16 type, S16 alpha, S16 beta, S16 gamma) {
    ASSERT_EQ_INT(type, group->Type);
    ASSERT_EQ_INT(alpha, group->Alpha);
    ASSERT_EQ_INT(beta, group->Beta);
    ASSERT_EQ_INT(gamma, group->Gamma);
}

static void setup_interp(T_OBJ_3D *obj) {
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
    obj->Interpolator = 0x8000; /* 50% = 0x8000 out of 0x10000 */
}

static void test_cpp_interp_50pct(void) {
    T_OBJ_3D obj;
    setup_interp(&obj);
    ObjectSetInterFrame(&obj);
    assert_group_eq(&obj.CurrentFrame[0], TYPE_ROTATE, 150, 300, 450);
    assert_group_eq(&obj.CurrentFrame[1], TYPE_TRANSLATE, 0, 0, 0);
}

static void test_cpp_interp_0pct(void) {
    T_OBJ_3D obj;
    setup_interp(&obj);
    obj.Interpolator = 0; /* 0% = fully "last" frame */
    ObjectSetInterFrame(&obj);
    assert_group_eq(&obj.CurrentFrame[0], TYPE_ROTATE, 100, 200, 300);
    assert_group_eq(&obj.CurrentFrame[1], TYPE_TRANSLATE, 1000, -2000, 3000);
}

static void test_cpp_interp_25pct(void) {
    T_OBJ_3D obj;
    setup_interp(&obj);
    obj.Interpolator = 0x4000; /* 25% */
    ObjectSetInterFrame(&obj);
    assert_group_eq(&obj.CurrentFrame[0], TYPE_ROTATE, 125, 250, 375);
    assert_group_eq(&obj.CurrentFrame[1], TYPE_TRANSLATE, 500, -1000, 1500);
}

static void test_cpp_interp_75pct(void) {
    T_OBJ_3D obj;
    setup_interp(&obj);
    obj.Interpolator = 0xC000; /* 75% */
    ObjectSetInterFrame(&obj);
    assert_group_eq(&obj.CurrentFrame[0], TYPE_ROTATE, 175, 350, 525);
    assert_group_eq(&obj.CurrentFrame[1], TYPE_TRANSLATE, -500, 1000, -1500);
}

static void test_cpp_interp_ffffpct(void) {
    T_OBJ_3D obj;
    setup_interp(&obj);
    obj.Interpolator = 0xFFFF; /* fixed-point endpoint, not exact frame copy */
    ObjectSetInterFrame(&obj);
    assert_group_eq(&obj.CurrentFrame[0], TYPE_ROTATE, 199, 399, 599);
    assert_group_eq(&obj.CurrentFrame[1], TYPE_TRANSLATE, -1000, 1999, -3000);
}

static void test_cpp_no_interp_when_flag_frame(void) {
    T_OBJ_3D obj;
    T_GROUP_INFO saved_frame[2];
    setup_interp(&obj);
    obj.Status = FLAG_CHANGE | FLAG_FRAME; /* FLAG_FRAME blocks interpolation */
    memcpy(saved_frame, obj.CurrentFrame, sizeof(saved_frame));
    ObjectSetInterFrame(&obj);
    ASSERT_MEM_EQ(saved_frame, obj.CurrentFrame, sizeof(saved_frame));
}

static void test_asm_equiv(void) {
    T_OBJ_3D cpp_obj, asm_obj;
    TransFctAnim = NULL;

    setup_interp(&cpp_obj);
    ObjectSetInterFrame(&cpp_obj);

    setup_interp(&asm_obj);
    asm_ObjectSetInterFrame(&asm_obj);

    ASSERT_ASM_CPP_MEM_EQ((U8 *)&asm_obj, (U8 *)&cpp_obj, sizeof(T_OBJ_3D),
                          "ObjectSetInterFrame 50%");
}

static void test_asm_equiv_25pct(void) {
    T_OBJ_3D cpp_obj, asm_obj;
    TransFctAnim = NULL;

    setup_interp(&cpp_obj);
    cpp_obj.Interpolator = 0x4000; /* 25% */
    ObjectSetInterFrame(&cpp_obj);

    setup_interp(&asm_obj);
    asm_obj.Interpolator = 0x4000;
    asm_ObjectSetInterFrame(&asm_obj);

    ASSERT_ASM_CPP_MEM_EQ((U8 *)&asm_obj, (U8 *)&cpp_obj, sizeof(T_OBJ_3D),
                          "ObjectSetInterFrame 25%");
}

static void test_asm_equiv_ffffpct(void) {
    T_OBJ_3D cpp_obj, asm_obj;
    TransFctAnim = NULL;

    setup_interp(&cpp_obj);
    cpp_obj.Interpolator = 0xFFFF;
    ObjectSetInterFrame(&cpp_obj);

    setup_interp(&asm_obj);
    asm_obj.Interpolator = 0xFFFF;
    asm_ObjectSetInterFrame(&asm_obj);

    ASSERT_ASM_CPP_MEM_EQ((U8 *)&asm_obj, (U8 *)&cpp_obj, sizeof(T_OBJ_3D),
                          "ObjectSetInterFrame 0xFFFF");
}

static void test_asm_equiv_random_interpolators(void) {
    static U32 rng_state;
    T_OBJ_3D cpp_obj, asm_obj;

    rng_state = 0xDEADBEEFu;
    for (int i = 0; i < 100; ++i) {
        U16 interpolator;
        rng_state = rng_state * 1103515245u + 12345u;
        interpolator = (U16)(rng_state >> 16);

        setup_interp(&cpp_obj);
        cpp_obj.Interpolator = interpolator;
        ObjectSetInterFrame(&cpp_obj);

        setup_interp(&asm_obj);
        asm_obj.Interpolator = interpolator;
        asm_ObjectSetInterFrame(&asm_obj);

        char label[96];
        snprintf(label, sizeof(label), "ObjectSetInterFrame random #%d interp=%u", i, interpolator);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)&asm_obj, (U8 *)&cpp_obj, sizeof(T_OBJ_3D), label);
    }
}

int main(void) {
    RUN_TEST(test_cpp_interp_50pct);
    RUN_TEST(test_cpp_interp_0pct);
    RUN_TEST(test_cpp_interp_25pct);
    RUN_TEST(test_cpp_interp_75pct);
    RUN_TEST(test_cpp_interp_ffffpct);
    RUN_TEST(test_cpp_no_interp_when_flag_frame);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_asm_equiv_25pct);
    RUN_TEST(test_asm_equiv_ffffpct);
    RUN_TEST(test_asm_equiv_random_interpolators);
    TEST_SUMMARY();
    return test_failures != 0;
}
