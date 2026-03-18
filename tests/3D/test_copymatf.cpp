/* Test: CopyMatrixF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/COPYMAT.H>
#include <string.h>

extern void CopyMatrixF(TYPE_MAT *d, TYPE_MAT *s);

extern "C" void asm_CopyMatrixF(void);
static void call_asm_CopyMatrixF(TYPE_MAT *d, TYPE_MAT *s) {
    __asm__ __volatile__("call asm_CopyMatrixF" : : "D"(d), "S"(s) : "memory");
}

static U32 rng_state;

static void rng_seed(U32 seed)
{
    rng_state = seed;
}

static U32 rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state;
}

static void set_matrix_words(TYPE_MAT *matrix, const U32 *words)
{
    memcpy(matrix, words, sizeof(TYPE_MAT));
}

static void set_matrix_floats(TYPE_MAT *matrix, const float *values)
{
    memcpy(&matrix->F.M11, values, sizeof(TYPE_MAT));
}

static void assert_copy_case(const char *label, const TYPE_MAT *src)
{
    TYPE_MAT cpp_dst;
    TYPE_MAT asm_dst;
    TYPE_MAT cpp_src;
    TYPE_MAT asm_src;

    memcpy(&cpp_src, src, sizeof(TYPE_MAT));
    memcpy(&asm_src, src, sizeof(TYPE_MAT));

    memset(&cpp_dst, 0xCC, sizeof(TYPE_MAT));
    memset(&asm_dst, 0xCC, sizeof(TYPE_MAT));

    CopyMatrixF(&cpp_dst, &cpp_src);
    call_asm_CopyMatrixF(&asm_dst, &asm_src);

    ASSERT_ASM_CPP_MEM_EQ(&asm_dst, &cpp_dst, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(&asm_src, &cpp_src, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(&asm_src, src, sizeof(TYPE_MAT), label);
}

static void test_fixed_identity_like(void)
{
    static const U32 words[sizeof(TYPE_MAT) / sizeof(U32)] = {
        0x3F800000u, 0x00000000u, 0x00000000u,
        0x00000000u, 0x3F800000u, 0x00000000u,
        0x00000000u, 0x00000000u, 0x3F800000u,
        0x41200000u, 0x41A00000u, 0x41F00000u
    };
    TYPE_MAT src;

    set_matrix_words(&src, words);
    assert_copy_case("CopyMatrixF identity-like", &src);
}

static void test_fixed_mixed_signs(void)
{
    static const U32 words[sizeof(TYPE_MAT) / sizeof(U32)] = {
        0xBF8CCCCDu, 0x400CCCCDu, 0xC0533333u,
        0x408CCCCDu, 0xC0B00000u, 0x40D33333u,
        0xC0F66666u, 0x410CCCCDu, 0xC11E6666u,
        0x41300000u, 0xC1420000u, 0x41540000u
    };
    TYPE_MAT src;

    set_matrix_words(&src, words);
    assert_copy_case("CopyMatrixF mixed signs", &src);
}

static void test_edge_finite_values(void)
{
    static const float values[sizeof(TYPE_MAT) / sizeof(float)] = {
        0.0f, -0.0f, 1.0e-30f,
        -1.0e-30f, 1.0e30f, -1.0e30f,
        65504.0f, -65504.0f, 3.1415927f,
        -2.7182817f, 0.5f, -0.5f
    };
    TYPE_MAT src;

    set_matrix_floats(&src, values);
    assert_copy_case("CopyMatrixF finite edge values", &src);
}

static void test_edge_zero_matrix(void)
{
    TYPE_MAT src;

    memset(&src, 0, sizeof(src));
    assert_copy_case("CopyMatrixF zero matrix", &src);
}

static void test_random_equivalence(void)
{
    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 200; ++round)
    {
        TYPE_MAT src;
        float values[sizeof(TYPE_MAT) / sizeof(float)];

        for (U32 index = 0; index < (sizeof(TYPE_MAT) / sizeof(float)); ++index)
        {
            S32 sample = (S32)(rng_next() & 0x7FFFu) - 0x3FFF;
            values[index] = (float)sample / 128.0f;
        }

        set_matrix_floats(&src, values);
        assert_copy_case("CopyMatrixF random", &src);
    }
}

int main(void)
{
    RUN_TEST(test_fixed_identity_like);
    RUN_TEST(test_fixed_mixed_signs);
    RUN_TEST(test_edge_finite_values);
    RUN_TEST(test_edge_zero_matrix);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
