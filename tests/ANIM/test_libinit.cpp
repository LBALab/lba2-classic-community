/* Test: InitObjects / ClearObjects — animation library state init */
#include "test_harness.h"

#include <ANIM/LIBINIT.H>
#include <OBJECT/AFF_OBJ.H>
#include <string.h>

extern "C" void asm_InitObjects(void *buffer, U32 size, Func_TransNumPtr *fctbody, Func_TransNumPtr *fctanim);
extern "C" void asm_ClearObjects(void);
extern "C" Func_TransNumPtr *asm_TransFctAnim;
extern "C" U8 *asm_Lib3DBufferAnim;
extern "C" U8 *asm_EndLib3DBufferAnim;
extern "C" U8 *asm_PtrLib3DBufferAnim;

extern void ClearObjects(void);

static void *dummy_trans(S32 num) { (void)num; return NULL; }
static void *dummy_trans_alt(S32 num) { return (void *)(intptr_t)(num + 1); }

typedef struct {
    Func_TransNumPtr *body;
    Func_TransNumPtr *anim;
    U8 *lib;
    U8 *end;
    U8 *ptr;
} AnimLibState;

static AnimLibState capture_cpp_state(void)
{
    AnimLibState state;
    state.body = TransFctBody;
    state.anim = TransFctAnim;
    state.lib = Lib3DBufferAnim;
    state.end = EndLib3DBufferAnim;
    state.ptr = PtrLib3DBufferAnim;
    return state;
}

static AnimLibState capture_asm_state(void)
{
    AnimLibState state;
    state.body = TransFctBody;
    state.anim = asm_TransFctAnim;
    state.lib = asm_Lib3DBufferAnim;
    state.end = asm_EndLib3DBufferAnim;
    state.ptr = asm_PtrLib3DBufferAnim;
    return state;
}

static void restore_cpp_state(const AnimLibState *state)
{
    TransFctBody = state->body;
    TransFctAnim = state->anim;
    Lib3DBufferAnim = state->lib;
    EndLib3DBufferAnim = state->end;
    PtrLib3DBufferAnim = state->ptr;
}

static void restore_asm_state(const AnimLibState *state)
{
    TransFctBody = state->body;
    asm_TransFctAnim = state->anim;
    asm_Lib3DBufferAnim = state->lib;
    asm_EndLib3DBufferAnim = state->end;
    asm_PtrLib3DBufferAnim = state->ptr;
}

static void assert_state_eq(const AnimLibState *expected, const AnimLibState *actual, const char *label)
{
    ASSERT_ASM_CPP_MEM_EQ(expected, actual, sizeof(AnimLibState), label);
}

static void assert_initobjects_case(const char *label, const AnimLibState *initial_state,
                                    void *buffer, U32 size,
                                    Func_TransNumPtr *body_fct,
                                    Func_TransNumPtr *anim_fct)
{
    AnimLibState asm_state;
    AnimLibState cpp_state;

    restore_asm_state(initial_state);
    asm_InitObjects(buffer, size, body_fct, anim_fct);
    asm_state = capture_asm_state();

    restore_cpp_state(initial_state);
    InitObjects(buffer, size, body_fct, anim_fct);
    cpp_state = capture_cpp_state();

    assert_state_eq(&asm_state, &cpp_state, label);
}

static void assert_clearobjects_case(const char *label, const AnimLibState *initial_state)
{
    AnimLibState asm_state;
    AnimLibState cpp_state;

    restore_asm_state(initial_state);
    asm_ClearObjects();
    asm_state = capture_asm_state();

    restore_cpp_state(initial_state);
    ClearObjects();
    cpp_state = capture_cpp_state();

    assert_state_eq(&asm_state, &cpp_state, label);
}

static void test_equivalence(void)
{
    U8 buffer[4096];
    U8 other_buffer[8192];
    AnimLibState initial;

    memset(buffer, 0x11, sizeof(buffer));
    memset(other_buffer, 0x22, sizeof(other_buffer));

    initial.body = NULL;
    initial.anim = NULL;
    initial.lib = NULL;
    initial.end = NULL;
    initial.ptr = NULL;
    assert_initobjects_case("InitObjects buffer", &initial,
                            buffer, sizeof(buffer),
                            (Func_TransNumPtr *)dummy_trans,
                            (Func_TransNumPtr *)dummy_trans_alt);

    initial.body = (Func_TransNumPtr *)dummy_trans_alt;
    initial.anim = (Func_TransNumPtr *)dummy_trans;
    initial.lib = other_buffer;
    initial.ptr = other_buffer + 32;
    initial.end = other_buffer + 256;
    assert_initobjects_case("InitObjects overwrite existing buffer state", &initial,
                            buffer, sizeof(buffer),
                            (Func_TransNumPtr *)dummy_trans,
                            (Func_TransNumPtr *)dummy_trans_alt);

    initial.body = (Func_TransNumPtr *)dummy_trans;
    initial.anim = (Func_TransNumPtr *)dummy_trans_alt;
    initial.lib = other_buffer;
    initial.ptr = other_buffer + 64;
    initial.end = other_buffer + 512;
    assert_initobjects_case("InitObjects null buffer already initialized", &initial,
                            NULL, sizeof(buffer),
                            (Func_TransNumPtr *)dummy_trans_alt,
                            (Func_TransNumPtr *)dummy_trans);

    assert_clearobjects_case("ClearObjects no malloc buffer", &initial);
}

int main(void)
{
    RUN_TEST(test_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
