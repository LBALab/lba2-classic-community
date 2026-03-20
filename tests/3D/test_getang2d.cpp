/* Test: GetAngleVector2D - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/GETANG2D.H>

extern "C" S32 asm_GetAngleVector2D(S32 x, S32 z);

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void assert_angle_case(const char *label, S32 x, S32 z) {
    S32 cpp = GetAngleVector2D(x, z);
    S32 asr = asm_GetAngleVector2D(x, z);
    ASSERT_ASM_CPP_EQ_INT(asr, cpp, label);
}

static void test_equivalence(void) {
    struct {
        S32 x, z;
    } cases[] = {
        {0, 0},
        {0, 1},
        {1, 0},
        {0, -1},
        {-1, 0},
        {100, 100},
        {-100, 100},
        {100, -100},
        {-100, -100},
        {1023, 1024},
        {1024, 1023},
        {-1023, 1024},
        {1023, -1024},
        {1, 1000},
        {1000, 1},
        {-500, 300},
        {300, -500},
        {2147483647, 1},
        {1, 2147483647},
        {-2147483647, 2147483647},
        {2147483647, -2147483647},
        {-100000, -100000},
    };
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "GetAngleVector2D fixed x=%d z=%d", cases[i].x, cases[i].z);
        assert_angle_case(lbl, cases[i].x, cases[i].z);
    }
}

static void test_boundary_equivalence(void) {
    struct {
        S32 x, z;
    } cases[] = {
        {1, 1},
        {1, 2},
        {2, 1},
        {-1, 1},
        {1, -1},
        {-1, -1},
        {-1, 2},
        {2, -1},
        {4095, 4096},
        {4096, 4095},
        {-4095, 4096},
        {4096, -4095},
        {7, 8},
        {8, 7},
        {-7, 8},
        {8, -7},
    };

    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "GetAngleVector2D boundary x=%d z=%d", cases[i].x, cases[i].z);
        assert_angle_case(lbl, cases[i].x, cases[i].z);
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        S32 x = (S32)rng_next() - 0x4000;
        S32 z = (S32)rng_next() - 0x4000;
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "GetAngleVector2D rand x=%d z=%d", x, z);
        assert_angle_case(lbl, x, z);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_boundary_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
