/* Test: LongInverseRotatePointF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>
#include <string.h>

extern void LongInverseRotatePointF(TYPE_MAT *m, S32 x, S32 y, S32 z);
extern "C" void asm_LongInverseRotatePointF(void);
static void call_asm_LongInverseRotatePointF(TYPE_MAT *m, S32 x, S32 y, S32 z) {
    __asm__ __volatile__("call asm_LongInverseRotatePointF" : : "S"(m), "a"(x), "b"(y), "c"(z) : "memory");
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

static void set_matrix(TYPE_MAT *m, const float *values)
{
    memset(m, 0, sizeof(*m));
    memcpy(&m->F.M11, values, 9 * sizeof(float));
}

static void assert_inverse_rotate_case(const char *label, const TYPE_MAT *source_matrix, S32 x, S32 y, S32 z)
{
    TYPE_MAT cpp_matrix;
    TYPE_MAT asm_matrix;

    memcpy(&cpp_matrix, source_matrix, sizeof(cpp_matrix));
    memcpy(&asm_matrix, source_matrix, sizeof(asm_matrix));

    LongInverseRotatePointF(&cpp_matrix, x, y, z);
    S32 cpp_x0 = X0;
    S32 cpp_y0 = Y0;
    S32 cpp_z0 = Z0;

    call_asm_LongInverseRotatePointF(&asm_matrix, x, y, z);

    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, label);
    ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, label);
    ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z0, label);
    ASSERT_ASM_CPP_MEM_EQ(source_matrix, &cpp_matrix, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(source_matrix, &asm_matrix, sizeof(TYPE_MAT), label);
}

static void test_equivalence(void)
{
    static const float matrices[][9] = {
        {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, 0.3f, 0.1f, 0.2f, 0.8f, 0.4f, 0.7f, 0.6f, 0.9f},
        {-0.75f, 0.25f, 0.5f, 0.125f, -0.5f, 0.75f, 0.625f, -0.25f, 0.375f},
    };
    struct { S32 x, y, z; } cases[] = {
        {0, 0, 0},
        {100, 200, 300},
        {-100, -200, -300},
        {1000, 0, 0},
        {0, 1000, 0},
        {0, 0, 1000},
        {1023, -1024, 511},
        {-32768, 16384, -8192},
    };

    for (int matrix_index = 0; matrix_index < (int)(sizeof(matrices) / sizeof(matrices[0])); ++matrix_index) {
        TYPE_MAT matrix;
        set_matrix(&matrix, matrices[matrix_index]);

        for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
            char lbl[128];
            snprintf(lbl, sizeof(lbl), "LongInverseRotatePointF fixed m=%d x=%d y=%d z=%d", matrix_index, cases[i].x, cases[i].y, cases[i].z);
            assert_inverse_rotate_case(lbl, &matrix, cases[i].x, cases[i].y, cases[i].z);
        }
    }
}

static void test_random_equivalence(void)
{
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; ++i) {
        TYPE_MAT matrix;
        float values[9];
        S32 x = ((S32)rng_next() << 1) - 0x4000;
        S32 y = ((S32)rng_next() << 1) - 0x4000;
        S32 z = ((S32)rng_next() << 1) - 0x4000;

        for (int value_index = 0; value_index < 9; ++value_index) {
            S32 sample = (S32)(rng_next() & 0x3FFu) - 512;
            values[value_index] = (float)sample / 256.0f;
        }

        set_matrix(&matrix, values);

        char lbl[128];
        snprintf(lbl, sizeof(lbl), "LongInverseRotatePointF rand x=%d y=%d z=%d", x, y, z);
        assert_inverse_rotate_case(lbl, &matrix, x, y, z);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
