/* Test: ObjectSetFrame — set object to specific animation frame */
#include "test_harness.h"
#include <ANIM/FRAME.H>
#include <ANIM/ANIM.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <ANIM.H>
#include <OBJECT/AFF_OBJ.H>
#include "anim_test_fixture.h"
#include <SYSTEM/TIMER.H>
#include <string.h>

extern "C" void asm_ObjectSetFrame(T_OBJ_3D *obj, U32 frame);

static U8 test_anim[512];

static void build_test_anim(void) {
    build_anim_header(test_anim, 3, 3, 1, 100);
    set_anim_group(test_anim, 3, 0, 1, 0, 100, 200, 300);
    set_anim_group(test_anim, 3, 0, 2, 0, 400, 500, 600);
    set_anim_group(test_anim, 3, 1, 1, 0, 110, 210, 310);
    set_anim_group(test_anim, 3, 1, 2, 0, 410, 510, 610);
    set_anim_group(test_anim, 3, 2, 1, 0, 120, 220, 320);
    set_anim_group(test_anim, 3, 2, 2, 0, 420, 520, 620);
}

static void setup_obj(T_OBJ_3D *obj) {
    build_test_anim();
    init_test_obj(obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    ObjectInitAnim(obj, test_anim);
}

static void seed_frame_state(T_OBJ_3D *obj) {
    obj->Interpolator = 0x12345678u;
    obj->LastAnimStepX = 111;
    obj->LastAnimStepY = 222;
    obj->LastAnimStepZ = 333;
    obj->LastAnimStepAlpha = 444;
    obj->LastAnimStepBeta = 555;
    obj->LastAnimStepGamma = 666;
    obj->LastOfsIsPtr = 1;
    obj->Status = FLAG_CHANGE | FLAG_BODY;
    obj->Time = 77;
    obj->LastTimer = 88;
}

static void test_set_frame0(void) {
    T_OBJ_3D obj;
    setup_obj(&obj);
    ObjectSetFrame(&obj, 0);
    ASSERT_EQ_INT(100, obj.CurrentFrame[0].Alpha);
    ASSERT_EQ_INT(200, obj.CurrentFrame[0].Beta);
    ASSERT_EQ_INT(300, obj.CurrentFrame[0].Gamma);
    ASSERT_EQ_INT(400, obj.CurrentFrame[1].Alpha);
}

static void test_set_frame1(void) {
    T_OBJ_3D obj;
    setup_obj(&obj);
    ObjectSetFrame(&obj, 1);
    ASSERT_EQ_INT(110, obj.CurrentFrame[0].Alpha);
    ASSERT_EQ_INT(210, obj.CurrentFrame[0].Beta);
    ASSERT_EQ_INT(310, obj.CurrentFrame[0].Gamma);
    ASSERT_EQ_INT(410, obj.CurrentFrame[1].Alpha);
}

static void test_set_frame2(void) {
    T_OBJ_3D obj;
    setup_obj(&obj);
    ObjectSetFrame(&obj, 2);
    ASSERT_EQ_INT(120, obj.CurrentFrame[0].Alpha);
    ASSERT_EQ_INT(220, obj.CurrentFrame[0].Beta);
    ASSERT_EQ_INT(320, obj.CurrentFrame[0].Gamma);
    ASSERT_EQ_INT(420, obj.CurrentFrame[1].Alpha);
}

static void test_asm_equiv_frame0(void) {
    T_OBJ_3D cpp_obj, asm_obj;
    TransFctAnim = NULL;

    setup_obj(&cpp_obj);
    seed_frame_state(&cpp_obj);
    TimerRefHR = 4000;
    ObjectSetFrame(&cpp_obj, 0);

    setup_obj(&asm_obj);
    seed_frame_state(&asm_obj);
    TimerRefHR = 4000;
    asm_ObjectSetFrame(&asm_obj, 0);

    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D),
                          "ObjectSetFrame full state frame 0");
}

static void test_asm_equiv_frame2(void) {
    T_OBJ_3D cpp_obj, asm_obj;
    TransFctAnim = NULL;

    setup_obj(&cpp_obj);
    seed_frame_state(&cpp_obj);
    TimerRefHR = 5000;
    ObjectSetFrame(&cpp_obj, 2);

    setup_obj(&asm_obj);
    seed_frame_state(&asm_obj);
    TimerRefHR = 5000;
    asm_ObjectSetFrame(&asm_obj, 2);

    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D),
                          "ObjectSetFrame full state frame 2");
}

static void test_frame_resets_state(void) {
    T_OBJ_3D obj;

    setup_obj(&obj);
    seed_frame_state(&obj);
    TimerRefHR = 1234;
    ObjectSetFrame(&obj, 1);

    ASSERT_EQ_INT(0, obj.Interpolator);
    ASSERT_EQ_INT(0, obj.LastAnimStepX);
    ASSERT_EQ_INT(0, obj.LastAnimStepY);
    ASSERT_EQ_INT(0, obj.LastAnimStepZ);
    ASSERT_EQ_INT(0, obj.LastAnimStepAlpha);
    ASSERT_EQ_INT(0, obj.LastAnimStepBeta);
    ASSERT_EQ_INT(0, obj.LastAnimStepGamma);
    ASSERT_EQ_INT(0, obj.LastOfsIsPtr);
    ASSERT_EQ_INT(FLAG_FRAME, obj.Status);
    ASSERT_EQ_INT(3, obj.NbGroups);
    ASSERT_EQ_INT(3, obj.LastNbGroups);
    ASSERT_EQ_INT(3, obj.NextNbGroups);
    ASSERT_EQ_UINT(1234u, obj.Time);
    ASSERT_EQ_UINT(1234u, obj.LastTimer);
}

static void test_asm_equiv_full_state_frame1(void) {
    T_OBJ_3D cpp_obj;
    T_OBJ_3D asm_obj;
    TransFctAnim = NULL;

    setup_obj(&cpp_obj);
    seed_frame_state(&cpp_obj);
    TimerRefHR = 4321;
    ObjectSetFrame(&cpp_obj, 1);

    setup_obj(&asm_obj);
    seed_frame_state(&asm_obj);
    TimerRefHR = 4321;
    asm_ObjectSetFrame(&asm_obj, 1);

    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D), "ObjectSetFrame full state frame 1");
}

int main(void) {
    RUN_TEST(test_set_frame0);
    RUN_TEST(test_set_frame1);
    RUN_TEST(test_set_frame2);
    RUN_TEST(test_asm_equiv_frame0);
    RUN_TEST(test_asm_equiv_frame2);
    RUN_TEST(test_frame_resets_state);
    RUN_TEST(test_asm_equiv_full_state_frame1);
    TEST_SUMMARY();
    return test_failures != 0;
}
