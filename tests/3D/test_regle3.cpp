/* Test: RegleTrois / BoundRegleTrois - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/REGLE3.H>

extern "C" S32 asm_RegleTrois(S32 v1, S32 v2, S32 steps, S32 step);
extern "C" S32 asm_BoundRegleTrois(S32 v1, S32 v2, S32 steps, S32 step);

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void assert_regle_case(const char *label, S32 v1, S32 v2, S32 steps, S32 step) {
    ASSERT_ASM_CPP_EQ_INT(asm_RegleTrois(v1, v2, steps, step), RegleTrois(v1, v2, steps, step), label);
    ASSERT_ASM_CPP_EQ_INT(asm_BoundRegleTrois(v1, v2, steps, step), BoundRegleTrois(v1, v2, steps, step), label);
}

static void test_equivalence(void) {
    struct {
        S32 v1, v2, steps, step;
    } cases[] = {
        {0, 100, 10, 0},
        {0, 100, 10, 5},
        {0, 100, 10, 10},
        {-100, 100, 4, 2},
        {0, 100, 0, 5},
        {0, 100, 0, 0},
        {0, 100, -3, 5},
        {0, 100, -3, -1},
        {0, 100, 1, 1},
        {-500, 500, 100, 73},
        {10, 90, 8, 0},
        {10, 90, 8, -5},
        {10, 90, 8, 100},
        {2147483000, 2147483600, 7, 3},
        {-2147483000, -2147482000, 9, 4},
    };
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "Regle3 fixed v1=%d v2=%d steps=%d step=%d",
                 cases[i].v1, cases[i].v2, cases[i].steps, cases[i].step);
        assert_regle_case(lbl, cases[i].v1, cases[i].v2, cases[i].steps, cases[i].step);
    }
}

static void test_boundary_equivalence(void) {
    struct {
        S32 v1, v2, steps, step;
    } cases[] = {
        {100, 200, 3, -1},
        {100, 200, 3, 0},
        {100, 200, 3, 1},
        {100, 200, 3, 2},
        {100, 200, 3, 3},
        {100, 200, 3, 4},
        {200, 100, 3, -1},
        {200, 100, 3, 0},
        {200, 100, 3, 1},
        {200, 100, 3, 2},
        {200, 100, 3, 3},
        {200, 100, 3, 4},
        {-7, 7, 2, 1},
        {-7, 7, 2, 2},
        {-7, 7, 2, 3},
        {100, 200, -1, -1},
        {100, 200, -1, 0},
        {100, 200, -1, 1},
    };

    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "Regle3 boundary v1=%d v2=%d steps=%d step=%d",
                 cases[i].v1, cases[i].v2, cases[i].steps, cases[i].step);
        assert_regle_case(lbl, cases[i].v1, cases[i].v2, cases[i].steps, cases[i].step);
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        S32 v1 = ((S32)rng_next() << 1) - 0x4000;
        S32 v2 = ((S32)rng_next() << 1) - 0x4000;
        S32 steps = (S32)(rng_next() % 257u) - 32;
        S32 step = (S32)(rng_next() % 513u) - 128;
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "Regle3 rand v1=%d v2=%d steps=%d step=%d", v1, v2, steps, step);
        assert_regle_case(lbl, v1, v2, steps, step);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_boundary_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
