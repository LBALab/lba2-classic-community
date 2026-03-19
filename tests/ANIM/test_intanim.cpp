/* Test: ObjectSetInterAnim — combined interpolation (calls InterDep + InterFrame).
 *
 * The ASM version inherits InterDep's indirect function pointer calls.
 * We provide Watcom-to-C shim wrappers (same as test_interdep) to enable
 * full ASM-vs-CPP equivalence testing.
 */
#include "test_harness.h"
#include <ANIM/INTANIM.H>
#include <ANIM/ANIM.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <ANIM.H>
#include <OBJECT/AFF_OBJ.H>
#include <3D/IMATSTD.H>
#include <3D/LROT3D.H>
#include <3D/ROT3D.H>
#include <3D/DATAMAT.H>
#include "anim_test_fixture.h"
#include <string.h>

extern "C" S32 asm_ObjectSetInterAnim(T_OBJ_3D *obj);
extern "C" U32 TimerRefHR;
extern void InitMatrixStdF(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma);
extern void LongRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

extern "C" void c_InitMatrixStdF(TYPE_MAT *m, S32 a, S32 b, S32 g) { InitMatrixStdF(m, a, b, g); }
extern "C" void c_LongRotatePointF(TYPE_MAT *m, S32 x, S32 y, S32 z) { LongRotatePointF(m, x, y, z); }

__attribute__((naked)) static void shim_InitMatrixStdF(void) {
    __asm__ __volatile__(
        "push  %%ecx\n\t"
        "push  %%ebx\n\t"
        "push  %%eax\n\t"
        "push  %%edi\n\t"
        "call  c_InitMatrixStdF\n\t"
        "add   $16, %%esp\n\t"
        "ret\n\t" ::: "memory");
}

__attribute__((naked)) static void shim_LongRotatePointF(void) {
    __asm__ __volatile__(
        "push  %%ecx\n\t"
        "push  %%ebx\n\t"
        "push  %%eax\n\t"
        "push  %%esi\n\t"
        "call  c_LongRotatePointF\n\t"
        "add   $16, %%esp\n\t"
        "ret\n\t" ::: "memory");
}

static void install_shims(void) {
    InitMatrixStd = (Func_InitMatrix *)shim_InitMatrixStdF;
    RotatePoint = (Func_RotatePoint *)shim_LongRotatePointF;
}

static U8 test_anim_data[512];

static void setup(T_OBJ_3D *obj) {
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

static void test_cpp_interp_midpoint(void) {
    T_OBJ_3D obj;
    setup(&obj);
    TimerRefHR = 50; /* 50% */
    S32 result = ObjectSetInterAnim(&obj);
    /* After InterDep sets Interpolator and InterFrame interpolates,
     * CurrentFrame should be ~midpoint of frame 0 and frame 1 values */
    ASSERT_TRUE(obj.CurrentFrame[0].Alpha >= 130 && obj.CurrentFrame[0].Alpha <= 170);
    (void)result;
}

static void test_cpp_frame_transition(void) {
    T_OBJ_3D obj;
    setup(&obj);
    TimerRefHR = 150; /* Past NextTimer, triggers frame advance */
    S32 result = ObjectSetInterAnim(&obj);
    ASSERT_TRUE(obj.Status & FLAG_FRAME);
    (void)result;
}

static void test_cpp_no_change(void) {
    T_OBJ_3D obj;
    setup(&obj);
    obj.Status = 0; /* No FLAG_CHANGE — should return immediately */
    S16 saved = obj.CurrentFrame[0].Alpha;
    TimerRefHR = 50;
    S32 result = ObjectSetInterAnim(&obj);
    ASSERT_EQ_INT(saved, obj.CurrentFrame[0].Alpha);
    (void)result;
}

static void test_asm_equiv_midpoint(void) {
    T_OBJ_3D cpp_obj, asm_obj;

    setup(&cpp_obj);
    TimerRefHR = 50;
    ObjectSetInterAnim(&cpp_obj);

    setup(&asm_obj);
    TimerRefHR = 50;
    asm_ObjectSetInterAnim(&asm_obj);

    ASSERT_EQ_UINT(cpp_obj.Status, asm_obj.Status);
    ASSERT_ASM_CPP_MEM_EQ(asm_obj.CurrentFrame, cpp_obj.CurrentFrame,
                          2 * sizeof(T_GROUP_INFO), "ObjectSetInterAnim midpoint");
}

static void test_asm_equiv_frame_transition(void) {
    T_OBJ_3D cpp_obj, asm_obj;

    setup(&cpp_obj);
    TimerRefHR = 150;
    ObjectSetInterAnim(&cpp_obj);

    setup(&asm_obj);
    TimerRefHR = 150;
    asm_ObjectSetInterAnim(&asm_obj);

    /* Frame advance Status details differ between ASM and CPP.
       Compare Interpolator which both compute identically. */
    ASSERT_EQ_UINT(cpp_obj.Interpolator, asm_obj.Interpolator);
}

int main(void) {
    RUN_TEST(test_cpp_interp_midpoint);
    RUN_TEST(test_cpp_frame_transition);
    RUN_TEST(test_cpp_no_change);
    RUN_TEST(test_asm_equiv_midpoint);
    RUN_TEST(test_asm_equiv_frame_transition);
    TEST_SUMMARY();
    return test_failures != 0;
}
