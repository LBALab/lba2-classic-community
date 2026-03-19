/* Test: ObjectInitBody — initialize body pointer for object */
#include "test_harness.h"

#include <ANIM/BODY.H>
#include <ANIM.H>
#include <string.h>

extern "C" void asm_ObjectInitBody(T_OBJ_3D *obj, void *body);

static U8 fake_body_a[64];
static U8 fake_body_b[64];
static U8 fake_body_c[64];

static U32 rng_state;

static void rng_seed(U32 seed)
{
    rng_state = seed;
}

static U32 rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void init_obj(T_OBJ_3D *obj, U32 status, S32 body_num, S32 next_body_num)
{
    memset(obj, 0xA5, sizeof(*obj));
    obj->Status = status;
    obj->Body.Num = body_num;
    obj->NextBody.Num = next_body_num;
}

static void assert_body_case(const char *label, const T_OBJ_3D *initial_obj, void *body)
{
    T_OBJ_3D cpp_obj;
    T_OBJ_3D asm_obj;

    memcpy(&cpp_obj, initial_obj, sizeof(cpp_obj));
    memcpy(&asm_obj, initial_obj, sizeof(asm_obj));

    ObjectInitBody(&cpp_obj, body);
    asm_ObjectInitBody(&asm_obj, body);

    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D), label);
}

static void test_equivalence(void)
{
    T_OBJ_3D initial;

    init_obj(&initial, 0, -1, 0);
    assert_body_case("ObjectInitBody first body", &initial, fake_body_a);

    init_obj(&initial, FLAG_FRAME, 1234, 5678);
    assert_body_case("ObjectInitBody existing body", &initial, fake_body_b);

    init_obj(&initial, FLAG_LAST_FRAME | FLAG_CHANGE, -1, -1);
    assert_body_case("ObjectInitBody preserve status bits", &initial, fake_body_c);

    init_obj(&initial, 0xFFFFFFFFu, 0, -1);
    assert_body_case("ObjectInitBody all-status-bits", &initial, fake_body_a);
}

static void test_random_equivalence(void)
{
    void *bodies[] = { fake_body_a, fake_body_b, fake_body_c };

    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; ++i) {
        T_OBJ_3D initial;
        U32 status = (rng_next() << 16) ^ rng_next();
        S32 body_num = (rng_next() & 1u) ? -1 : (S32)(((U32)rng_next() << 16) ^ rng_next());
        S32 next_body_num = (S32)(((U32)rng_next() << 16) ^ rng_next());
        char label[64];

        init_obj(&initial, status, body_num, next_body_num);
        snprintf(label, sizeof(label), "ObjectInitBody rand %d", i);
        assert_body_case(label, &initial, bodies[i % 3]);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
