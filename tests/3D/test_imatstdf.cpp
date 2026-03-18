/* Test: InitMatrixStdF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/IMATSTD.H>
#include <3D/SINTABF.H>

extern void InitMatrixStdF(TYPE_MAT *m, S32 a, S32 b, S32 g);

extern "C" void asm_InitMatrixStdF(void);
static void call_asm_InitMatrixStdF(TYPE_MAT *m, S32 a, S32 b, S32 g) {
    __asm__ __volatile__("call asm_InitMatrixStdF"
        : "+a"(a), "+b"(b), "+c"(g) : "D"(m) : "memory", "edx", "esi");
}

static U32 rng_state;

static void rng_seed(U32 seed)
{
    rng_state = seed;
}

static U32 rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void assert_matrix_case(const char *label, S32 a, S32 b, S32 g)
{
    TYPE_MAT cpp_matrix;
    TYPE_MAT asm_matrix;

    InitMatrixStdF(&cpp_matrix, a, b, g);
    call_asm_InitMatrixStdF(&asm_matrix, a, b, g);

    ASSERT_ASM_CPP_MEM_EQ(&asm_matrix, &cpp_matrix, sizeof(TYPE_MAT), label);
}

static void test_equivalence(void)
{
    struct { S32 a, b, g; } cases[] = {
        {0, 0, 0},
        {1024, 0, 0},
        {0, 1024, 0},
        {0, 0, 1024},
        {512, 512, 512},
        {1023, 1024, 1025},
        {2048, 2048, 2048},
        {3072, 1024, 2048},
        {4095, 4095, 4095},
        {4096, 4096, 4096},
        {-1, 0, 1},
        {-1024, 500, 3000},
        {-4096, 8192, -12288},
        {123, 2345, 3456},
        {8191, -8192, 12287},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "InitMatrixStdF fixed a=%d b=%d g=%d", cases[i].a, cases[i].b, cases[i].g);
        assert_matrix_case(lbl, cases[i].a, cases[i].b, cases[i].g);
    }
}

static void test_random_equivalence(void)
{
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        S32 a = (S32)rng_next() - 0x4000;
        S32 b = (S32)rng_next() - 0x4000;
        S32 g = (S32)rng_next() - 0x4000;
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "InitMatrixStdF rand a=%d b=%d g=%d", a, b, g);
        assert_matrix_case(lbl, a, b, g);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
