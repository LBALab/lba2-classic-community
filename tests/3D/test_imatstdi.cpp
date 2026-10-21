/* Test: InitMatrixStdI - ASM vs CPP equivalence (the 1997 integer path) */
#include "test_harness.h"
#include <3D/IMATSTD.H>
#include <string.h>

extern "C" void asm_InitMatrixStdI(void);
static void call_asm_InitMatrixStdI(TYPE_MAT *m, S32 a, S32 b, S32 g) {
    __asm__ __volatile__("call asm_InitMatrixStdI"
                         : "+a"(a), "+b"(b), "+c"(g) : "D"(m) : "memory", "edx", "esi");
}

static U32 rng_state;

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state;
}

static void assert_matrix_case(const char *label, S32 a, S32 b, S32 g) {
    TYPE_MAT cpp_matrix;
    TYPE_MAT asm_matrix;

    /* Different fill so a field one side leaves unwritten cannot match by luck. */
    memset(&cpp_matrix, 0xA5, sizeof(cpp_matrix));
    memset(&asm_matrix, 0x5A, sizeof(asm_matrix));

    InitMatrixStdI(&cpp_matrix, a, b, g);
    call_asm_InitMatrixStdI(&asm_matrix, a, b, g);

    ASSERT_ASM_CPP_MEM_EQ(&asm_matrix, &cpp_matrix, sizeof(TYPE_MAT), label);
}

/* Each angle through all 4096 steps with the other two held, at several holds. */
static void test_every_angle(void) {
    const S32 holds[] = {0, 1, 511, 1024, 1500, 2048, 3071, 4095};
    char lbl[96];
    for (int h = 0; h < (int)(sizeof(holds) / sizeof(holds[0])); h++) {
        for (S32 v = 0; v < 4096; v++) {
            snprintf(lbl, sizeof(lbl), "InitMatrixStdI alpha=%d hold=%d", v, holds[h]);
            assert_matrix_case(lbl, v, holds[h], holds[h]);
            snprintf(lbl, sizeof(lbl), "InitMatrixStdI beta=%d hold=%d", v, holds[h]);
            assert_matrix_case(lbl, holds[h], v, holds[h]);
            snprintf(lbl, sizeof(lbl), "InitMatrixStdI gamma=%d hold=%d", v, holds[h]);
            assert_matrix_case(lbl, holds[h], holds[h], v);
        }
    }
}

/* Angles outside 0..4095: the ASM masks with and 4095, negatives included. */
static void test_out_of_range_angles(void) {
    const S32 cases[][3] = {
        {-1, -1, -1},
        {-4096, 4096, 8191},
        {-300, 3700, -2048},
        {12345, -12345, 65535},
        {(S32)0x7FFFFFFF, (S32)0x80000000, -2},
    };
    char lbl[96];
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        snprintf(lbl, sizeof(lbl), "InitMatrixStdI a=%d b=%d g=%d", cases[i][0], cases[i][1], cases[i][2]);
        assert_matrix_case(lbl, cases[i][0], cases[i][1], cases[i][2]);
    }
}

static void test_random_angles(void) {
    rng_state = 0x1997C0DEu;
    char lbl[96];
    for (int i = 0; i < 100000; i++) {
        S32 a = (S32)(rng_next() >> 8) & 4095;
        S32 b = (S32)(rng_next() >> 8) & 4095;
        S32 g = (S32)(rng_next() >> 8) & 4095;
        snprintf(lbl, sizeof(lbl), "InitMatrixStdI rand a=%d b=%d g=%d", a, b, g);
        assert_matrix_case(lbl, a, b, g);
    }
}

int main(void) {
    RUN_TEST(test_every_angle);
    RUN_TEST(test_out_of_range_angles);
    RUN_TEST(test_random_angles);
    TEST_SUMMARY();
    return test_failures != 0;
}
