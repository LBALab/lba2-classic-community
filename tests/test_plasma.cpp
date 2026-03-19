#include "test_harness.h"

#include <PLASMA.H>

#include <string.h>

extern "C" void asm_Do_Plasma(T_PLASMA *PtrEffectStruct);
extern "C" U32 Nb_Pts_Inter;
extern "C" U32 Nb_Pts_Control;
extern "C" U32 asm_Nb_Pts_Inter;
extern "C" U32 asm_Nb_Pts_Control;

static U32 rng_state;

static void rng_seed(U32 seed_value) {
    rng_state = seed_value;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void fill_random_u8(U8 *buffer, U32 size) {
    for (U32 i = 0; i < size; ++i) {
        buffer[i] = (U8)rng_next();
    }
}

static void fill_random_s16(S16 *buffer, U32 count) {
    for (U32 i = 0; i < count; ++i) {
        buffer[i] = (S16)((S16)rng_next() - 0x4000);
    }
}

static void seed_control_grid(S16 *buffer, U32 nb_active_points, U32 nb_pts_inter) {
    const U32 width = nb_active_points * nb_pts_inter;
    const U32 total_count = width * nb_active_points;

    fill_random_s16(buffer, total_count);

    for (U32 row = 0; row < nb_active_points; ++row) {
        for (U32 col = 0; col < nb_active_points; ++col) {
            buffer[row * width + col * nb_pts_inter] = (S16)((S16)rng_next() - 0x4000);
        }
    }
}

static void run_plasma_case(const char *label, U8 interleave, U8 nb_active_points) {
    const U32 nb_pts_inter = 1u << interleave;
    const U32 width = nb_active_points * nb_pts_inter;
    const U32 tab_virgule_count = width * nb_active_points;
    const U32 tex_size = 256u * width;

    S16 cpp_tab_virgule[256];
    S16 asm_tab_virgule[256];
    S16 cpp_tab_speed[32];
    S16 asm_tab_speed[32];
    S32 cpp_tab_acc[32];
    S32 asm_tab_acc[32];
    U8 cpp_tab_colors[256];
    U8 asm_tab_colors[256];
    U8 cpp_tex[256 * 40];
    U8 asm_tex[256 * 40];
    T_PLASMA cpp_plasma;
    T_PLASMA asm_plasma;

    ASSERT_TRUE(tab_virgule_count <= (sizeof(cpp_tab_virgule) / sizeof(cpp_tab_virgule[0])));
    ASSERT_TRUE(tex_size <= sizeof(cpp_tex));

    seed_control_grid(cpp_tab_virgule, nb_active_points, nb_pts_inter);
    memcpy(asm_tab_virgule, cpp_tab_virgule, tab_virgule_count * sizeof(S16));

    fill_random_s16(cpp_tab_speed, sizeof(cpp_tab_speed) / sizeof(cpp_tab_speed[0]));
    memcpy(asm_tab_speed, cpp_tab_speed, sizeof(cpp_tab_speed));

    for (U32 i = 0; i < (sizeof(cpp_tab_acc) / sizeof(cpp_tab_acc[0])); ++i) {
        cpp_tab_acc[i] = (S32)(((U32)rng_next() << 16) | rng_next());
    }
    memcpy(asm_tab_acc, cpp_tab_acc, sizeof(cpp_tab_acc));

    fill_random_u8(cpp_tab_colors, sizeof(cpp_tab_colors));
    memcpy(asm_tab_colors, cpp_tab_colors, sizeof(cpp_tab_colors));

    memset(cpp_tex, 0xA5, sizeof(cpp_tex));
    memset(asm_tex, 0xA5, sizeof(asm_tex));

    memset(&cpp_plasma, 0, sizeof(cpp_plasma));
    cpp_plasma.TabVirgule = cpp_tab_virgule;
    cpp_plasma.TabSpeed = cpp_tab_speed;
    cpp_plasma.TabAcc = cpp_tab_acc;
    cpp_plasma.TabColors = cpp_tab_colors;
    cpp_plasma.TexOffset = cpp_tex;
    cpp_plasma.Interleave = interleave;
    cpp_plasma.NbActivePoints = nb_active_points;
    cpp_plasma.NbColors = 0xFF;
    cpp_plasma.Speed = (U8)rng_next();
    cpp_plasma.data_start = (U8)rng_next();

    memset(&asm_plasma, 0, sizeof(asm_plasma));
    asm_plasma.TabVirgule = asm_tab_virgule;
    asm_plasma.TabSpeed = asm_tab_speed;
    asm_plasma.TabAcc = asm_tab_acc;
    asm_plasma.TabColors = asm_tab_colors;
    asm_plasma.TexOffset = asm_tex;
    asm_plasma.Interleave = interleave;
    asm_plasma.NbActivePoints = nb_active_points;
    asm_plasma.NbColors = 0xFF;
    asm_plasma.Speed = cpp_plasma.Speed;
    asm_plasma.data_start = cpp_plasma.data_start;

    Nb_Pts_Inter = 0;
    Nb_Pts_Control = 0;
    Do_Plasma(&cpp_plasma);

    asm_Nb_Pts_Inter = 0;
    asm_Nb_Pts_Control = 0;
    asm_Do_Plasma(&asm_plasma);

    ASSERT_ASM_CPP_EQ_INT(asm_Nb_Pts_Inter, Nb_Pts_Inter, label);
    ASSERT_ASM_CPP_EQ_INT(asm_Nb_Pts_Control, Nb_Pts_Control, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_tab_virgule, cpp_tab_virgule, tab_virgule_count * sizeof(S16), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_tex, cpp_tex, tex_size, "Do_Plasma texture");
    ASSERT_ASM_CPP_MEM_EQ(asm_tab_colors, cpp_tab_colors, sizeof(cpp_tab_colors), "Do_Plasma colors");
    ASSERT_ASM_CPP_MEM_EQ(asm_tab_speed, cpp_tab_speed, sizeof(cpp_tab_speed), "Do_Plasma speed table");
    ASSERT_ASM_CPP_MEM_EQ(asm_tab_acc, cpp_tab_acc, sizeof(cpp_tab_acc), "Do_Plasma acc table");
}

static void test_plasma_fixed_cases(void) {
    rng_seed(0x11112222u);
    run_plasma_case("Do_Plasma interleave1 active3", 1, 3);
    run_plasma_case("Do_Plasma interleave2 active4", 2, 4);
    run_plasma_case("Do_Plasma interleave3 active3", 3, 3);
}

static void test_plasma_edge_cases(void) {
    rng_seed(0x33334444u);
    run_plasma_case("Do_Plasma interleave1 active2", 1, 2);
    run_plasma_case("Do_Plasma interleave3 active2", 3, 2);
}

static void test_plasma_random_stress(void) {
    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 300; ++round) {
        U8 interleave = (U8)((rng_next() % 3) + 1);
        U8 nb_active_points = (U8)((rng_next() % 4) + 2);

        run_plasma_case("Do_Plasma random", interleave, nb_active_points);
    }
}

int main(void) {
    RUN_TEST(test_plasma_fixed_cases);
    RUN_TEST(test_plasma_edge_cases);
    RUN_TEST(test_plasma_random_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}