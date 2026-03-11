/* Test: ObjectInitTexture — initialize texture pointer for object */
#include "test_harness.h"

#include <ANIM/TEXTURE.H>
#include <ANIM/CLEAR.H>
#include <string.h>

/* ASM ObjectInitTexture uses Watcom register convention:
   parm [ebx] [eax], modify exact [] */
extern "C" void asm_ObjectInitTexture(void);
static void call_asm_ObjectInitTexture(T_OBJ_3D *obj, void *texture) {
    __asm__ __volatile__(
        "call asm_ObjectInitTexture"
        : "+b"(obj), "+a"(texture)
        :
        : "memory");
}

static U8 fake_texture[64];

static void test_first_texture_sets_both(void)
{
    T_OBJ_3D obj;
    ObjectClear(&obj);
    ObjectInitTexture(&obj, fake_texture);
    /* When Texture was (void*)-1, Texture should also be set */
    ASSERT_TRUE(obj.Texture == fake_texture);
    ASSERT_TRUE(obj.NextTexture == fake_texture);
}

static void test_second_texture_only_next(void)
{
    T_OBJ_3D obj;
    ObjectClear(&obj);
    U8 tex1[64], tex2[64];
    ObjectInitTexture(&obj, tex1);
    ObjectInitTexture(&obj, tex2);
    ASSERT_TRUE(obj.Texture == tex1);
    ASSERT_TRUE(obj.NextTexture == tex2);
}

static void test_asm_equiv(void)
{
    T_OBJ_3D cpp_obj, asm_obj;
    ObjectClear(&cpp_obj);
    ObjectClear(&asm_obj);
    ObjectInitTexture(&cpp_obj, fake_texture);
    call_asm_ObjectInitTexture(&asm_obj, fake_texture);
    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D), "ObjectInitTexture");
}

int main(void)
{
    RUN_TEST(test_first_texture_sets_both);
    RUN_TEST(test_second_texture_only_next);
    RUN_TEST(test_asm_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
