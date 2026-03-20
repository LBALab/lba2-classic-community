/* Test: GetAngleVector3D - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/GETANG3D.H>
#include <3D/CAMERA.H>

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

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void assert_angle3d_case(const char *label, S32 x, S32 y, S32 z) {
    S32 cpp_ret = GetAngleVector3D(x, y, z);
    S32 cpp_x0 = X0;
    S32 cpp_y0 = Y0;
    S32 asr_ret = call_asm_GetAngleVector3D(x, y, z);

    ASSERT_ASM_CPP_EQ_INT(asr_ret, cpp_ret, label);
    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, label);
    ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, label);
}

static void test_equivalence(void) {
    struct {
        S32 x, y, z;
    } cases[] = {
        {0, 0, 0},
        {0, 0, 100},
        {100, 0, 0},
        {0, 100, 0},
        {1, 1, 1},
        {-1, 0, 1},
        {1000, 500, 200},
        {-100, -50, -200},
        {1024, 0, 1023},
        {1023, 0, 1024},
        {1024, 1024, 1024},
        {-1024, 1024, 1024},
        {1024, -1024, 1024},
        {32767, 16384, 1},
        {1, 16384, 32767},
        {-32767, -16384, 32767},
    };
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "GetAngleVector3D fixed x=%d y=%d z=%d", cases[i].x, cases[i].y, cases[i].z);
        assert_angle3d_case(lbl, cases[i].x, cases[i].y, cases[i].z);
    }
}

static void test_vertical_and_wrap_equivalence(void) {
    struct {
        S32 x, y, z;
    } cases[] = {
        {0, 1, 0},
        {0, -1, 0},
        {0, 32767, 0},
        {0, -32767, 0},
        {1, 32767, 0},
        {0, 32767, 1},
        {1, -32767, 1},
        {32767, 1, 32767},
        {-32767, 1, 32767},
        {32767, -1, -32767},
    };

    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        char lbl[112];
        snprintf(lbl, sizeof(lbl), "GetAngleVector3D vertical x=%d y=%d z=%d", cases[i].x, cases[i].y, cases[i].z);
        assert_angle3d_case(lbl, cases[i].x, cases[i].y, cases[i].z);
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        S32 x = (S32)rng_next() - 0x4000;
        S32 y = (S32)rng_next() - 0x4000;
        S32 z = (S32)rng_next() - 0x4000;
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "GetAngleVector3D rand x=%d y=%d z=%d", x, y, z);
        assert_angle3d_case(lbl, x, y, z);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_vertical_and_wrap_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
