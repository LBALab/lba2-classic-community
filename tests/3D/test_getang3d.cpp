/* Test: GetAngleVector3D - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/GETANG3D.H>
#include <3D/CAMERA.H>
#include <stdlib.h>

extern "C" S32 asm_GetAngleVector3D(S32 x, S32 y, S32 z);

static void test_equivalence(void)
{
    struct { S32 x, y, z; } cases[] = {
        {0, 0, 0}, {0, 0, 100}, {100, 0, 0}, {0, 100, 0},
        {-100, -50, -200}, {1, 1, 1}, {1000, 500, 200}, {-1, 0, 1},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        S32 cpp_ret = GetAngleVector3D(cases[i].x, cases[i].y, cases[i].z);
        S32 cpp_x0 = X0, cpp_y0 = Y0;
        S32 asr_ret = asm_GetAngleVector3D(cases[i].x, cases[i].y, cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(asr_ret, cpp_ret, "GetAngleVector3D ret");
        ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, "GetAngleVector3D X0");
        ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, "GetAngleVector3D Y0");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        S32 x = (S32)rand() - RAND_MAX / 2;
        S32 y = (S32)rand() - RAND_MAX / 2;
        S32 z = (S32)rand() - RAND_MAX / 2;
        S32 cpp_ret = GetAngleVector3D(x, y, z);
        S32 cpp_x0 = X0, cpp_y0 = Y0;
        S32 asr_ret = asm_GetAngleVector3D(x, y, z);
        ASSERT_ASM_CPP_EQ_INT(asr_ret, cpp_ret, "GetAngleVector3D rand ret");
        ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, "GetAngleVector3D rand X0");
        ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, "GetAngleVector3D rand Y0");
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
