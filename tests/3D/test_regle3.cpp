/* Test: RegleTrois / BoundRegleTrois - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/REGLE3.H>
#include <stdlib.h>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_RegleTrois(S32 v1, S32 v2, S32 steps, S32 step);
extern "C" S32 asm_BoundRegleTrois(S32 v1, S32 v2, S32 steps, S32 step);

static void test_equivalence(void)
{
    struct { S32 v1, v2, steps, step; } cases[] = {
        {0, 100, 10, 0}, {0, 100, 10, 5}, {0, 100, 10, 10},
        {-100, 100, 4, 2}, {0, 100, 0, 5}, {0, 100, 1, 1},
        {-500, 500, 100, 73}, {10, 90, 8, 0}, {10, 90, 8, -5}, {10, 90, 8, 100},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        S32 v1=cases[i].v1, v2=cases[i].v2, s=cases[i].steps, st=cases[i].step;
        ASSERT_ASM_CPP_EQ_INT(asm_RegleTrois(v1,v2,s,st), RegleTrois(v1,v2,s,st), "RegleTrois");
        ASSERT_ASM_CPP_EQ_INT(asm_BoundRegleTrois(v1,v2,s,st), BoundRegleTrois(v1,v2,s,st), "BoundRegleTrois");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        S32 v1 = (S32)rand() - RAND_MAX/2;
        S32 v2 = (S32)rand() - RAND_MAX/2;
        S32 steps = (rand() % 100) + 1;
        S32 step = rand() % (steps + 1);
        ASSERT_ASM_CPP_EQ_INT(asm_RegleTrois(v1,v2,steps,step), RegleTrois(v1,v2,steps,step), "RegleTrois rand");
        ASSERT_ASM_CPP_EQ_INT(asm_BoundRegleTrois(v1,v2,steps,step), BoundRegleTrois(v1,v2,steps,step), "BoundRegleTrois rand");
    }
}
#endif

int main(void)
{
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
#else
    printf("SKIPPED - ASM tests not enabled\n");
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
