/* Test: ObjectInitTexture — initialize texture pointer for object */
#include "test_harness.h"

#include <ANIM/TEXTURE.H>
#include <ANIM.H>
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

static U8 fake_texture_a[64];
static U8 fake_texture_b[64];
static U8 fake_texture_c[64];

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void init_obj(T_OBJ_3D *obj, void *texture, void *next_texture) {
    memset(obj, 0xA5, sizeof(*obj));
    obj->Texture = texture;
    obj->NextTexture = next_texture;
}

static void assert_texture_case(const char *label, const T_OBJ_3D *initial_obj, void *texture) {
    T_OBJ_3D cpp_obj;
    T_OBJ_3D asm_obj;

    memcpy(&cpp_obj, initial_obj, sizeof(cpp_obj));
    memcpy(&asm_obj, initial_obj, sizeof(asm_obj));

    ObjectInitTexture(&cpp_obj, texture);
    call_asm_ObjectInitTexture(&asm_obj, texture);

    ASSERT_ASM_CPP_MEM_EQ(&asm_obj, &cpp_obj, sizeof(T_OBJ_3D), label);
}

static void test_equivalence(void) {
    T_OBJ_3D initial;

    init_obj(&initial, (void *)-1, (void *)-1);
    assert_texture_case("ObjectInitTexture first texture", &initial, fake_texture_a);

    init_obj(&initial, fake_texture_b, (void *)-1);
    assert_texture_case("ObjectInitTexture existing texture", &initial, fake_texture_c);

    init_obj(&initial, NULL, fake_texture_a);
    assert_texture_case("ObjectInitTexture null current texture", &initial, fake_texture_b);

    init_obj(&initial, fake_texture_c, fake_texture_b);
    assert_texture_case("ObjectInitTexture overwrite next texture", &initial, fake_texture_a);
}

static void test_random_equivalence(void) {
    void *textures[] = {fake_texture_a, fake_texture_b, fake_texture_c, NULL, (void *)-1};

    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; ++i) {
        T_OBJ_3D initial;
        void *texture = textures[rng_next() % 5u];
        void *next_texture = textures[rng_next() % 5u];
        void *new_texture = textures[rng_next() % 3u];
        char label[64];

        init_obj(&initial, texture, next_texture);
        snprintf(label, sizeof(label), "ObjectInitTexture rand %d", i);
        assert_texture_case(label, &initial, new_texture);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
