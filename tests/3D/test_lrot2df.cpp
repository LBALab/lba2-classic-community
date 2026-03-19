/* Test: LongRotate - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROT2D.H>
#include <3D/CAMERA.H>

/* ASM LongRotateF uses Watcom register convention:
   parm [eax] [ecx] [edx], modify exact [edx] */
extern "C" void asm_LongRotateF(void);
static void call_asm_LongRotateF(S32 x, S32 z, S32 angle) {
    __asm__ __volatile__(
        "call asm_LongRotateF"
        : "+a"(x), "+c"(z), "+d"(angle)
        :
        : "memory");
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void assert_rotate_case(const char *label, S32 x, S32 z, S32 angle) {
    LongRotate(x, z, angle);
    S32 cpp_x0 = X0;
    S32 cpp_z0 = Z0;
    call_asm_LongRotateF(x, z, angle);

    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, label);
    ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z0, label);
}

static void test_equivalence(void) {
    struct {
        S32 x, z, angle;
    } cases[] = {
        {0, 0, 0},
        {100, 0, 0},
        {100, 200, 0},
        {100, 0, 1024},
        {100, 200, 2048},
        {100, 200, 4096},
        {100, 200, -4096},
        {100, 200, 4097},
        {100, 200, -1},
        {-100, -200, 512},
        {500, 300, 3072},
        {1024, -1024, 1023},
        {-2048, 4096, 2049},
    };
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "LongRotate fixed x=%d z=%d a=%d", cases[i].x, cases[i].z, cases[i].angle);
        assert_rotate_case(lbl, cases[i].x, cases[i].z, cases[i].angle);
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        S32 x = ((S32)rng_next() << 1) - 0x4000;
        S32 z = ((S32)rng_next() << 1) - 0x4000;
        S32 a = (S32)(rng_next() % 16384u) - 8192;
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "LongRotate rand x=%d z=%d a=%d", x, z, a);
        assert_rotate_case(lbl, x, z, a);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
