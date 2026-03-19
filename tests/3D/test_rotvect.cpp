/* Test: RotateVector - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROTVECT.H>
#include <3D/CAMERA.H>

/* ASM RotateVector uses Watcom register convention:
   parm [edx] [eax] [ebx] [ecx], modify [esi edi] */
extern "C" void asm_RotateVector(void);
static void call_asm_RotateVector(S32 n, S32 a, S32 b, S32 g) {
    __asm__ __volatile__(
        "call asm_RotateVector"
        : "+a"(a), "+b"(b), "+c"(g), "+d"(n)
        :
        : "memory", "esi", "edi");
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void assert_rotatevector_case(const char *label, S32 n, S32 a, S32 b, S32 g) {
    RotateVector(n, a, b, g);
    S32 cpp_x = X0;
    S32 cpp_y = Y0;
    S32 cpp_z = Z0;

    call_asm_RotateVector(n, a, b, g);

    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x, label);
    ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y, label);
    ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z, label);
}

static void test_equivalence(void) {
    struct {
        S32 n, a, b, g;
    } cases[] = {
        {0, 0, 0, 0},
        {100, 0, 0, 0},
        {100, 1024, 0, 0},
        {100, 0, 1024, 0},
        {100, 0, 0, 1024},
        {500, 512, 512, 512},
        {-100, 2048, 1024, 3072},
        {32767, 4096, 0, 0},
        {-32768, -1024, 8192, -4096},
        {12345, 4095, 4094, 4093},
    };
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        char label[96];
        snprintf(label, sizeof(label), "RotateVector fixed n=%d a=%d b=%d g=%d",
                 cases[i].n, cases[i].a, cases[i].b, cases[i].g);
        assert_rotatevector_case(label, cases[i].n, cases[i].a, cases[i].b, cases[i].g);
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        S32 n = (S32)(((U32)rng_next() << 16) | rng_next());
        S32 a = (S32)(rng_next() % 16384u) - 8192;
        S32 b = (S32)(rng_next() % 16384u) - 8192;
        S32 g = (S32)(rng_next() % 16384u) - 8192;
        char label[64];

        snprintf(label, sizeof(label), "RotateVector rand %d", i);
        assert_rotatevector_case(label, n, a, b, g);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
