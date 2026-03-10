/* Test: RegleTrois and BoundRegleTrois — linear interpolation */
#include "test_harness.h"

#include <3D/REGLE3.H>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_RegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep);
extern "C" S32 asm_BoundRegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep);
#endif

static void test_regle_midpoint(void)
{
    ASSERT_EQ_INT(50, RegleTrois(0, 100, 2, 1));
}

static void test_regle_start(void)
{
    ASSERT_EQ_INT(0, RegleTrois(0, 100, 10, 0));
}

static void test_regle_end(void)
{
    ASSERT_EQ_INT(100, RegleTrois(0, 100, 10, 10));
}

static void test_regle_negative_vals(void)
{
    ASSERT_EQ_INT(-50, RegleTrois(-100, 0, 2, 1));
}

static void test_regle_nbsteps_zero(void)
{
    /* NbSteps <= 0 returns 0 in CPP */
    ASSERT_EQ_INT(0, RegleTrois(10, 20, 0, 5));
    ASSERT_EQ_INT(0, RegleTrois(10, 20, -1, 5));
}

static void test_bound_clamp_low(void)
{
    /* CurrentStep <= 0 → return Val1 */
    ASSERT_EQ_INT(10, BoundRegleTrois(10, 90, 8, 0));
    ASSERT_EQ_INT(10, BoundRegleTrois(10, 90, 8, -5));
}

static void test_bound_clamp_high(void)
{
    /* CurrentStep >= NbSteps → return Val2 */
    ASSERT_EQ_INT(90, BoundRegleTrois(10, 90, 8, 8));
    ASSERT_EQ_INT(90, BoundRegleTrois(10, 90, 8, 100));
}

static void test_bound_midpoint(void)
{
    ASSERT_EQ_INT(50, BoundRegleTrois(0, 100, 2, 1));
}

#ifdef LBA2_ASM_TESTS
static void test_regle_asm_equiv(void)
{
    struct { S32 v1, v2, steps, step; } cases[] = {
        {0, 100, 10, 0}, {0, 100, 10, 5}, {0, 100, 10, 10},
        {-100, 100, 4, 2}, {0, 100, 0, 5}, {0, 100, 1, 1},
        {-500, 500, 100, 73},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        S32 cpp = RegleTrois(cases[i].v1, cases[i].v2, cases[i].steps, cases[i].step);
        S32 asm_r = asm_RegleTrois(cases[i].v1, cases[i].v2, cases[i].steps, cases[i].step);
        ASSERT_ASM_CPP_EQ_INT(asm_r, cpp, "RegleTrois");
    }
}

static void test_bound_asm_equiv(void)
{
    struct { S32 v1, v2, steps, step; } cases[] = {
        {10, 90, 8, 0}, {10, 90, 8, 4}, {10, 90, 8, 8},
        {10, 90, 8, -5}, {10, 90, 8, 100},
        {-100, 100, 10, 5}, {0, 1000, 3, 2},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        S32 cpp = BoundRegleTrois(cases[i].v1, cases[i].v2, cases[i].steps, cases[i].step);
        S32 asm_r = asm_BoundRegleTrois(cases[i].v1, cases[i].v2, cases[i].steps, cases[i].step);
        ASSERT_ASM_CPP_EQ_INT(asm_r, cpp, "BoundRegleTrois");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_regle_midpoint);
    RUN_TEST(test_regle_start);
    RUN_TEST(test_regle_end);
    RUN_TEST(test_regle_negative_vals);
    RUN_TEST(test_regle_nbsteps_zero);
    RUN_TEST(test_bound_clamp_low);
    RUN_TEST(test_bound_clamp_high);
    RUN_TEST(test_bound_midpoint);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_regle_asm_equiv);
    RUN_TEST(test_bound_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
