/* Test: ObjectClear — zero/sentinel-fill T_OBJ_3D struct */
#include "test_harness.h"

#include <ANIM/CLEAR.H>
#include <string.h>

extern "C" void asm_ObjectClear(T_OBJ_3D *obj);

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void fill_random_bytes(void *buffer, size_t size) {
    U8 *bytes = (U8 *)buffer;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = (U8)(rng_next() & 0xFFu);
    }
}

static void assert_clear_case(const char *label, const T_OBJ_3D *initial_obj) {
    T_OBJ_3D cpp_obj;
    T_OBJ_3D asm_obj;

    memcpy(&cpp_obj, initial_obj, sizeof(cpp_obj));
    memcpy(&asm_obj, initial_obj, sizeof(asm_obj));

    ObjectClear(&cpp_obj);
    asm_ObjectClear(&asm_obj);

    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D), label);
}

static void test_equivalence(void) {
    T_OBJ_3D initial;

    memset(&initial, 0x00, sizeof(initial));
    assert_clear_case("ObjectClear zero-filled", &initial);

    memset(&initial, 0xFF, sizeof(initial));
    assert_clear_case("ObjectClear ff-filled", &initial);

    memset(&initial, 0xAA, sizeof(initial));
    assert_clear_case("ObjectClear aa-filled", &initial);

    memset(&initial, 0x55, sizeof(initial));
    assert_clear_case("ObjectClear 55-filled", &initial);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; ++i) {
        T_OBJ_3D initial;
        char label[64];

        fill_random_bytes(&initial, sizeof(initial));
        snprintf(label, sizeof(label), "ObjectClear rand %d", i);
        assert_clear_case(label, &initial);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
