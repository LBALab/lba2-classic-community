#include "test_harness.h"

#include "FIRE.H"

#include <SYSTEM/ADELINE_TYPES.H>

#include <string.h>

extern "C" void asm_Do_Fire(U8 *ptrwork1, U8 *ptrwork2, U8 *colortable, U8 *ptrtex);
extern U32 FireSeed;

static U32 rng_state;

static void rng_seed(U32 seed_value) {
    rng_state = seed_value;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void fill_constant(U8 *buffer, U32 size, U8 value) {
    memset(buffer, value, size);
}

static void fill_sequential(U8 *buffer, U32 size, U8 base) {
    for (U32 i = 0; i < size; ++i) {
        buffer[i] = (U8)(base + (i & 0xFFu));
    }
}

static void fill_checker(U8 *buffer, U32 width, U32 height, U8 a, U8 b) {
    for (U32 y = 0; y < height; ++y) {
        for (U32 x = 0; x < width; ++x) {
            buffer[y * width + x] = ((x ^ y) & 1u) ? a : b;
        }
    }
}

static void fill_vertical_stripes(U8 *buffer, U32 width, U32 height, U8 a, U8 b) {
    for (U32 y = 0; y < height; ++y) {
        for (U32 x = 0; x < width; ++x) {
            buffer[y * width + x] = (x & 1u) ? a : b;
        }
    }
}

static void fill_random(U8 *buffer, U32 size) {
    for (U32 i = 0; i < size; ++i) {
        buffer[i] = (U8)rng_next();
    }
}

static void run_fire_equivalence(const U8 *initial_work1,
                                 const U8 *initial_work2,
                                 const U8 *initial_colortable,
                                 const U8 *initial_tex,
                                 const char *work1_label) {
    U8 cpp_work1[32 * 36];
    U8 cpp_work2[32 * 36];
    U8 cpp_tex[32 * 256];
    U8 asm_work1[32 * 36];
    U8 asm_work2[32 * 36];
    U8 asm_tex[32 * 256];
    U8 cpp_colortable[32];
    U8 asm_colortable[32];
    memcpy(cpp_work1, initial_work1, sizeof(cpp_work1));
    memcpy(cpp_work2, initial_work2, sizeof(cpp_work2));
    memcpy(cpp_tex, initial_tex, sizeof(cpp_tex));
    memcpy(cpp_colortable, initial_colortable, sizeof(cpp_colortable));
    Do_Fire(cpp_work1, cpp_work2, cpp_colortable, cpp_tex);
    Do_Fire(cpp_work1, cpp_work2, cpp_colortable, cpp_tex);

    memcpy(asm_work1, initial_work1, sizeof(asm_work1));
    memcpy(asm_work2, initial_work2, sizeof(asm_work2));
    memcpy(asm_tex, initial_tex, sizeof(asm_tex));
    memcpy(asm_colortable, initial_colortable, sizeof(asm_colortable));
    asm_Do_Fire(asm_work1, asm_work2, asm_colortable, asm_tex);
    asm_Do_Fire(asm_work1, asm_work2, asm_colortable, asm_tex);

    ASSERT_ASM_CPP_MEM_EQ(asm_work1, cpp_work1, sizeof(cpp_work1), work1_label);
    ASSERT_ASM_CPP_MEM_EQ(asm_work2, cpp_work2, sizeof(cpp_work2), "Do_Fire work2");
    ASSERT_ASM_CPP_MEM_EQ(asm_tex, cpp_tex, sizeof(cpp_tex), "Do_Fire texture");
    ASSERT_ASM_CPP_MEM_EQ(asm_colortable, cpp_colortable, sizeof(cpp_colortable), "Do_Fire colortable");
}

static void test_fire_zero_inputs(void) {
    U8 work1[32 * 36];
    U8 work2[32 * 36];
    U8 colortable[32];
    U8 tex[32 * 256];

    fill_constant(work1, sizeof(work1), 0x00);
    fill_constant(work2, sizeof(work2), 0x00);
    fill_sequential(colortable, sizeof(colortable), 0x10);
    fill_constant(tex, sizeof(tex), 0x00);

    run_fire_equivalence(work1, work2, colortable, tex, "Do_Fire zero work1");
}

static void test_fire_all_ff_inputs(void) {
    U8 work1[32 * 36];
    U8 work2[32 * 36];
    U8 colortable[32];
    U8 tex[32 * 256];

    fill_constant(work1, sizeof(work1), 0xFF);
    fill_constant(work2, sizeof(work2), 0xFF);
    fill_sequential(colortable, sizeof(colortable), 0xC0);
    fill_constant(tex, sizeof(tex), 0xAA);

    run_fire_equivalence(work1, work2, colortable, tex, "Do_Fire ff work1");
}

static void test_fire_checkerboard_inputs(void) {
    U8 work1[32 * 36];
    U8 work2[32 * 36];
    U8 colortable[32];
    U8 tex[32 * 256];

    fill_checker(work1, 32, 36, 0x20, 0xD0);
    fill_checker(work2, 32, 36, 0x11, 0xEE);
    fill_sequential(colortable, sizeof(colortable), 0x40);
    fill_constant(tex, sizeof(tex), 0x55);

    run_fire_equivalence(work1, work2, colortable, tex, "Do_Fire checker work1");
}

static void test_fire_sequential_inputs(void) {
    U8 work1[32 * 36];
    U8 work2[32 * 36];
    U8 colortable[32];
    U8 tex[32 * 256];

    fill_sequential(work1, sizeof(work1), 0x01);
    fill_sequential(work2, sizeof(work2), 0x80);
    fill_sequential(colortable, sizeof(colortable), 0x00);
    fill_sequential(tex, sizeof(tex), 0x33);

    run_fire_equivalence(work1, work2, colortable, tex, "Do_Fire sequential work1");
}

static void test_fire_vertical_stripe_inputs(void) {
    U8 work1[32 * 36];
    U8 work2[32 * 36];
    U8 colortable[32];
    U8 tex[32 * 256];

    fill_vertical_stripes(work1, 32, 36, 0x08, 0xF0);
    fill_vertical_stripes(work2, 32, 36, 0x7F, 0x40);
    fill_sequential(colortable, sizeof(colortable), 0x20);
    fill_checker(tex, 32, 256, 0x12, 0xE1);

    run_fire_equivalence(work1, work2, colortable, tex, "Do_Fire vertical stripes work1");
}

static void test_fire_edge_impulse_inputs(void) {
    U8 work1[32 * 36];
    U8 work2[32 * 36];
    U8 colortable[32];
    U8 tex[32 * 256];

    fill_constant(work1, sizeof(work1), 0x00);
    fill_constant(work2, sizeof(work2), 0x00);
    fill_constant(tex, sizeof(tex), 0x00);
    fill_sequential(colortable, sizeof(colortable), 0x60);

    work1[1 * 32 + 0] = 0xFF;
    work1[1 * 32 + 31] = 0x80;
    work1[16 * 32 + 0] = 0x40;
    work2[2 * 32 + 31] = 0xFF;
    work2[20 * 32 + 0] = 0x7F;
    tex[0] = 0xAA;
    tex[31] = 0x55;

    run_fire_equivalence(work1, work2, colortable, tex, "Do_Fire edge impulse work1");
}

static void test_fire_random_stress(void) {
    U8 work1[32 * 36];
    U8 work2[32 * 36];
    U8 colortable[32];
    U8 tex[32 * 256];

    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 300; ++round) {
        fill_random(work1, sizeof(work1));
        fill_random(work2, sizeof(work2));
        fill_random(colortable, sizeof(colortable));
        fill_random(tex, sizeof(tex));

        run_fire_equivalence(work1,
                             work2,
                             colortable,
                             tex,
                             "Do_Fire random work1");
    }
}

int main(void) {
    RUN_TEST(test_fire_zero_inputs);
    RUN_TEST(test_fire_all_ff_inputs);
    RUN_TEST(test_fire_checkerboard_inputs);
    RUN_TEST(test_fire_sequential_inputs);
    RUN_TEST(test_fire_vertical_stripe_inputs);
    RUN_TEST(test_fire_edge_impulse_inputs);
    RUN_TEST(test_fire_random_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}