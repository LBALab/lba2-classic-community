/* Test: GetAngleVector3D - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/GETANG3D.H>
#include <3D/CAMERA.H>
#include <stdlib.h>

/* ASM GetAngleVector3D uses Watcom register convention:
   parm [eax] [ebx] [ecx], modify [edx esi edi] */
extern "C" void asm_GetAngleVector3D(void);
static S32 call_asm_GetAngleVector3D(S32 x, S32 y, S32 z) {
    S32 result;
    __asm__ __volatile__(
        "call asm_GetAngleVector3D"
        : "=a"(result)
        : "a"(x), "b"(y), "c"(z)
        : "memory", "edx", "esi", "edi");
    return result;
}

static void test_equivalence(void)
{
    struct { S32 x, y, z; } cases[] = {
        {0, 0, 0}, {0, 0, 100}, {100, 0, 0}, {0, 100, 0},
        {-100, -50, -200}, {1, 1, 1}, {1000, 500, 200}, {-1, 0, 1},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        S32 cpp_ret = GetAngleVector3D(cases[i].x, cases[i].y, cases[i].z);
        S32 cpp_x0 = X0, cpp_y0 = Y0;
        S32 asr_ret = call_asm_GetAngleVector3D(cases[i].x, cases[i].y, cases[i].z);
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
        S32 asr_ret = call_asm_GetAngleVector3D(x, y, z);
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
