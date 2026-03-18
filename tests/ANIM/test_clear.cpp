/* Test: ObjectClear — zero/sentinel-fill T_OBJ_3D struct */
#include "test_harness.h"

#include <ANIM/CLEAR.H>
#include <string.h>

extern "C" void asm_ObjectClear(T_OBJ_3D *obj);

static void test_clears_to_zero(void)
{
    T_OBJ_3D obj;
    memset(&obj, 0xFF, sizeof(obj));
    ObjectClear(&obj);
    ASSERT_EQ_INT(0, obj.X);
    ASSERT_EQ_INT(0, obj.Y);
    ASSERT_EQ_INT(0, obj.Z);
    ASSERT_EQ_INT(0, obj.Alpha);
    ASSERT_EQ_INT(0, obj.Status);
}

static void test_sentinels(void)
{
    T_OBJ_3D obj;
    memset(&obj, 0, sizeof(obj));
    ObjectClear(&obj);
    ASSERT_EQ_INT(-1, obj.Body.Num);
    ASSERT_EQ_INT(-1, obj.NextBody.Num);
    ASSERT_EQ_INT(-1, obj.Anim.Num);
    ASSERT_TRUE(obj.Texture == (void *)-1);
    ASSERT_TRUE(obj.NextTexture == (void *)-1);
}

static void test_idempotent(void)
{
    T_OBJ_3D obj;
    ObjectClear(&obj);
    ObjectClear(&obj);
    ASSERT_EQ_INT(-1, obj.Body.Num);
    ASSERT_EQ_INT(0, obj.X);
}

static void test_asm_equiv(void)
{
    T_OBJ_3D cpp_obj, asm_obj;
    memset(&cpp_obj, 0xAA, sizeof(cpp_obj));
    memset(&asm_obj, 0xAA, sizeof(asm_obj));
    ObjectClear(&cpp_obj);
    asm_ObjectClear(&asm_obj);
    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D), "ObjectClear");
}

int main(void)
{
    RUN_TEST(test_clears_to_zero);
    RUN_TEST(test_sentinels);
    RUN_TEST(test_idempotent);
    RUN_TEST(test_asm_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
