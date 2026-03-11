/* Test: GetAngleVector2D - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/GETANG2D.H>
#include <stdlib.h>

extern "C" S32 asm_GetAngleVector2D(S32 x, S32 z);

static void test_equivalence(void)
{
    struct { S32 x, z; } cases[] = {
        {0, 0}, {0, 1}, {1, 0}, {0, -1}, {-1, 0},
        {100, 100}, {-100, 100}, {100, -100}, {-100, -100},
        {1, 1000}, {1000, 1}, {-500, 300}, {300, -500},
        {100000, 1}, {1, 100000}, {-100000, -100000},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        S32 cpp = GetAngleVector2D(cases[i].x, cases[i].z);
        S32 asr = asm_GetAngleVector2D(cases[i].x, cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(asr, cpp, "GetAngleVector2D");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        S32 x = (S32)rand() - RAND_MAX / 2;
        S32 z = (S32)rand() - RAND_MAX / 2;
        S32 cpp = GetAngleVector2D(x, z);
        S32 asr = asm_GetAngleVector2D(x, z);
        ASSERT_ASM_CPP_EQ_INT(asr, cpp, "GetAngleVector2D rand");
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
