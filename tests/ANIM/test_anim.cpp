/* Test: ObjectInitAnim — initialize object animation from binary anim data */
#include "test_harness.h"
#include <ANIM/ANIM.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <ANIM.H>
#include <OBJECT/AFF_OBJ.H>
#include "anim_test_fixture.h"
#include <string.h>
#include <stdio.h>

extern "C" void asm_ObjectInitAnim(T_OBJ_3D *obj, void *anim);

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

static U8 test_anim_data[512];

static void build_test_anim(void) {
    build_anim_header(test_anim_data, 3, 3, 1, 100);
    set_anim_group(test_anim_data, 3, 0, 1, 0, 100, 200, 300);
    set_anim_group(test_anim_data, 3, 0, 2, 0, 400, 500, 600);
    set_anim_group(test_anim_data, 3, 1, 1, 0, 110, 210, 310);
    set_anim_group(test_anim_data, 3, 1, 2, 0, 410, 510, 610);
    set_anim_group(test_anim_data, 3, 2, 1, 0, 120, 220, 320);
    set_anim_group(test_anim_data, 3, 2, 2, 0, 420, 520, 620);
}

static void test_cpp_init_basic(void) {
    T_OBJ_3D obj;
    build_test_anim();
    init_test_obj(&obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    ObjectInitAnim(&obj, test_anim_data);
    ASSERT_EQ_UINT(3, obj.NbFrames);
    ASSERT_EQ_UINT(3, obj.NbGroups);
    ASSERT_EQ_INT(1, obj.LoopFrame);
    ASSERT_TRUE(obj.Status & FLAG_FRAME);
    ASSERT_TRUE(obj.Anim.Ptr == test_anim_data);
}

static void test_cpp_currentframe(void) {
    T_OBJ_3D obj;
    build_test_anim();
    init_test_obj(&obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    ObjectInitAnim(&obj, test_anim_data);
    ASSERT_EQ_INT(100, obj.CurrentFrame[0].Alpha);
    ASSERT_EQ_INT(200, obj.CurrentFrame[0].Beta);
    ASSERT_EQ_INT(300, obj.CurrentFrame[0].Gamma);
    ASSERT_EQ_INT(400, obj.CurrentFrame[1].Alpha);
}

static void test_cpp_single_frame(void) {
    U8 anim1[512];
    T_OBJ_3D obj;
    build_anim_header(anim1, 1, 2, 0, 50);
    set_anim_group(anim1, 2, 0, 1, 0, 42, 84, 126);
    init_test_obj(&obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    ObjectInitAnim(&obj, anim1);
    ASSERT_EQ_UINT(1, obj.NbFrames);
    ASSERT_EQ_UINT(2, obj.NbGroups);
    ASSERT_EQ_INT(42, obj.CurrentFrame[0].Alpha);
}

static void test_asm_equiv(void) {
    T_OBJ_3D cpp_obj, asm_obj;
    build_test_anim();
    TransFctAnim = NULL;

    init_test_obj(&cpp_obj);
    init_anim_buffer();
    ObjectInitAnim(&cpp_obj, test_anim_data);

    init_test_obj(&asm_obj);
    init_anim_buffer();
    asm_ObjectInitAnim(&asm_obj, test_anim_data);

    ASSERT_EQ_UINT(cpp_obj.NbFrames, asm_obj.NbFrames);
    ASSERT_EQ_UINT(cpp_obj.NbGroups, asm_obj.NbGroups);
    ASSERT_EQ_INT(cpp_obj.LoopFrame, asm_obj.LoopFrame);
    ASSERT_EQ_UINT(cpp_obj.Status, asm_obj.Status);
    ASSERT_ASM_CPP_MEM_EQ(asm_obj.CurrentFrame, cpp_obj.CurrentFrame,
                          2 * sizeof(T_GROUP_INFO), "ObjectInitAnim CurrentFrame");
}

static void test_random_batch(void) {
    rng_seed(0xA01B0001);
    int prev = test_failures;
    for (int i = 0; i < 20 && test_failures == prev; i++) {
        U8 anim[1024];
        U16 nf = (U16)(rng_next() % 5) + 1;
        U16 ng = (U16)(rng_next() % 4) + 2;
        U16 lf = (U16)(rng_next() % nf);
        U16 dt = (U16)(rng_next() % 200) + 10;
        build_anim_header(anim, nf, ng, lf, dt);
        for (U16 f = 0; f < nf; f++)
            for (U16 g = 0; g < ng; g++)
                set_anim_group(anim, ng, f, g, 0,
                               (S16)(rng_next() % 4096), (S16)(rng_next() % 4096), (S16)(rng_next() % 4096));

        T_OBJ_3D cpp_obj, asm_obj;
        TransFctAnim = NULL;

        init_test_obj(&cpp_obj);
        init_anim_buffer();
        ObjectInitAnim(&cpp_obj, anim);

        init_test_obj(&asm_obj);
        init_anim_buffer();
        asm_ObjectInitAnim(&asm_obj, anim);

        char label[64];
        snprintf(label, sizeof(label), "InitAnim random #%d nf=%d ng=%d", i, nf, ng);
        ASSERT_EQ_UINT(cpp_obj.NbFrames, asm_obj.NbFrames);
        ASSERT_EQ_UINT(cpp_obj.NbGroups, asm_obj.NbGroups);
        U32 cf_size = (ng > 1) ? (ng - 1) * sizeof(T_GROUP_INFO) : 0;
        if (cf_size > 0)
            ASSERT_ASM_CPP_MEM_EQ(asm_obj.CurrentFrame, cpp_obj.CurrentFrame, cf_size, label);
    }
}

int main(void) {
    RUN_TEST(test_cpp_init_basic);
    RUN_TEST(test_cpp_currentframe);
    RUN_TEST(test_cpp_single_frame);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_random_batch);
    TEST_SUMMARY();
    return test_failures != 0;
}
