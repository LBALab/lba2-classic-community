/* Test: LongRotatePointF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROT3D.H>
#include <3D/CAMERA.H>
#include <string.h>

extern void LongRotatePointF(TYPE_MAT *m, S32 x, S32 y, S32 z);
extern "C" void asm_LongRotatePointF(void);
static void call_asm_LongRotatePointF(TYPE_MAT *m, S32 x, S32 y, S32 z) {
    __asm__ __volatile__("call asm_LongRotatePointF" : : "S"(m), "a"(x), "b"(y), "c"(z) : "memory");
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static TYPE_MAT make_test_mat(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 0.5f;
    m.F.M12 = 0.3f;
    m.F.M13 = 0.1f;
    m.F.M21 = 0.2f;
    m.F.M22 = 0.8f;
    m.F.M23 = 0.4f;
    m.F.M31 = 0.7f;
    m.F.M32 = 0.6f;
    m.F.M33 = 0.9f;
    return m;
}

static TYPE_MAT make_alt_test_mat(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = -0.75f;
    m.F.M12 = 0.25f;
    m.F.M13 = 0.5f;
    m.F.M21 = 0.125f;
    m.F.M22 = -0.5f;
    m.F.M23 = 0.75f;
    m.F.M31 = 0.625f;
    m.F.M32 = -0.25f;
    m.F.M33 = 0.375f;
    return m;
}

static TYPE_MAT make_identity_mat(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 1.0f;
    m.F.M22 = 1.0f;
    m.F.M33 = 1.0f;
    return m;
}

static void assert_rotate3d_case(const char *label, const TYPE_MAT *source_matrix, S32 x, S32 y, S32 z) {
    TYPE_MAT cpp_matrix;
    TYPE_MAT asm_matrix;

    memcpy(&cpp_matrix, source_matrix, sizeof(cpp_matrix));
    memcpy(&asm_matrix, source_matrix, sizeof(asm_matrix));

    LongRotatePointF(&cpp_matrix, x, y, z);
    S32 cpp_x0 = X0;
    S32 cpp_y0 = Y0;
    S32 cpp_z0 = Z0;

    call_asm_LongRotatePointF(&asm_matrix, x, y, z);

    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, label);
    ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, label);
    ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z0, label);
    ASSERT_ASM_CPP_MEM_EQ(source_matrix, &cpp_matrix, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(source_matrix, &asm_matrix, sizeof(TYPE_MAT), label);
}

static void test_equivalence(void) {
    TYPE_MAT matrices[] = {
        make_identity_mat(),
        make_test_mat(),
        make_alt_test_mat(),
    };
    struct {
        S32 x, y, z;
    } cases[] = {
        {0, 0, 0},
        {100, 200, 300},
        {-100, -200, -300},
        {1000, 0, 0},
        {0, 1000, 0},
        {0, 0, 1000},
        {-5000, 3000, -1000},
        {1023, -1024, 511},
        {-32768, 16384, -8192},
    };
    for (int matrix_index = 0; matrix_index < (int)(sizeof(matrices) / sizeof(matrices[0])); ++matrix_index) {
        for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
            char lbl[128];
            snprintf(lbl, sizeof(lbl), "LongRotatePointF fixed m=%d x=%d y=%d z=%d",
                     matrix_index, cases[i].x, cases[i].y, cases[i].z);
            assert_rotate3d_case(lbl, &matrices[matrix_index], cases[i].x, cases[i].y, cases[i].z);
        }
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        TYPE_MAT m;
        float *values = &m.F.M11;
        memset(&m, 0, sizeof(m));

        for (int index = 0; index < 9; ++index) {
            S32 sample = (S32)(rng_next() & 0x3FFu) - 512;
            values[index] = (float)sample / 256.0f;
        }

        S32 x = ((S32)rng_next() << 1) - 0x4000;
        S32 y = ((S32)rng_next() << 1) - 0x4000;
        S32 z = ((S32)rng_next() << 1) - 0x4000;
        char lbl[128];
        snprintf(lbl, sizeof(lbl), "LongRotatePointF rand x=%d y=%d z=%d", x, y, z);
        assert_rotate3d_case(lbl, &m, x, y, z);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
