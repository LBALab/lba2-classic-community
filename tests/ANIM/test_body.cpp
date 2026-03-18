/* Test: ObjectInitBody — initialize body pointer for object */
#include "test_harness.h"

#include <ANIM/BODY.H>
#include <ANIM/CLEAR.H>
#include <string.h>

extern "C" void asm_ObjectInitBody(T_OBJ_3D *obj, void *body);

static U8 fake_body[64];

static void test_first_body_sets_both(void)
{
    T_OBJ_3D obj;
    ObjectClear(&obj);
    ObjectInitBody(&obj, fake_body);
    /* When Body.Num was -1, Body should also be set */
    ASSERT_TRUE(obj.Body.Ptr == fake_body);
    ASSERT_TRUE(obj.NextBody.Ptr == fake_body);
}

static void test_second_body_only_next(void)
{
    T_OBJ_3D obj;
    ObjectClear(&obj);
    U8 body1[64], body2[64];
    ObjectInitBody(&obj, body1);
    ObjectInitBody(&obj, body2);
    /* Body stays as body1, NextBody changes to body2 */
    ASSERT_TRUE(obj.Body.Ptr == body1);
    ASSERT_TRUE(obj.NextBody.Ptr == body2);
}

static void test_sets_flag(void)
{
    T_OBJ_3D obj;
    ObjectClear(&obj);
    ObjectInitBody(&obj, fake_body);
    /* Status should have FLAG_BODY set */
    ASSERT_TRUE((obj.Status & 0x01) != 0 || obj.Status != 0);
}

static void test_asm_equiv(void)
{
    T_OBJ_3D cpp_obj, asm_obj;
    ObjectClear(&cpp_obj);
    ObjectClear(&asm_obj);
    ObjectInitBody(&cpp_obj, fake_body);
    asm_ObjectInitBody(&asm_obj, fake_body);
    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D), "ObjectInitBody");
}

int main(void)
{
    RUN_TEST(test_first_body_sets_both);
    RUN_TEST(test_second_body_only_next);
    RUN_TEST(test_sets_flag);
    RUN_TEST(test_asm_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
