/* Test: Sqr / QSqr - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/SQRROOT.H>

/* ASM Sqr uses Watcom register convention: parm [eax], result in eax */
extern "C" void asm_Sqr(void);
static U32 call_asm_Sqr(U32 x) {
    U32 result;
    __asm__ __volatile__(
        "call asm_Sqr"
        : "=a"(result)
        : "a"(x)
        : "memory", "ebx", "ecx", "edx", "esi", "edi");
    return result;
}

/* ASM QSqr uses Watcom register convention: parm [eax] [edx], result in eax */
extern "C" void asm_QSqr(void);
static U32 call_asm_QSqr(U32 xLow, U32 xHigh) {
    U32 result;
    __asm__ __volatile__(
        "call asm_QSqr"
        : "=a"(result)
        : "a"(xLow), "d"(xHigh)
        : "memory", "ebx", "ecx", "esi", "edi");
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

static U32 rng_next_u32(void) {
    return (rng_next() << 17) ^ (rng_next() << 2) ^ (rng_next() & 0x3u);
}

static void test_equivalence(void) {
    U32 sqr_cases[] = {0, 1, 2, 3, 4, 9, 100, 10000, 1000000, 0xFFFFFFFF};
    for (int i = 0; i < (int)(sizeof(sqr_cases) / sizeof(sqr_cases[0])); i++) {
        ASSERT_ASM_CPP_EQ_INT(call_asm_Sqr(sqr_cases[i]), Sqr(sqr_cases[i]), "Sqr");
    }
    struct {
        U32 lo, hi;
    } qsqr_cases[] = {
        {0, 0},
        {1, 0},
        {100, 0},
        {0, 1},
        {0, 2},
        {0xFFFFFFFFu, 0},
        {83682999u, 268440030u},
        {0xFFFFFFFFu, 0xFFFFFFFFu},
        {0x12345678u, 0x9ABCDEF0u},
    };
    for (int i = 0; i < (int)(sizeof(qsqr_cases) / sizeof(qsqr_cases[0])); i++) {
        U32 cpp = QSqr(qsqr_cases[i].lo, qsqr_cases[i].hi);
        U32 asr = call_asm_QSqr(qsqr_cases[i].lo, qsqr_cases[i].hi);
        ASSERT_ASM_CPP_EQ_INT(asr, cpp, "QSqr");
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        U32 x = rng_next_u32();
        U32 lo = rng_next_u32();
        U32 hi = rng_next_u32();

        ASSERT_ASM_CPP_EQ_INT(call_asm_Sqr(x), Sqr(x), "Sqr rand");
        ASSERT_ASM_CPP_EQ_INT(call_asm_QSqr(lo, hi), QSqr(lo, hi), "QSqr rand");
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
