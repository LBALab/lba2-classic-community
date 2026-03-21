/* Test: RotateMatrixU - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROTMAT.H>
#include <3D/IMATSTD.H>
#include <string.h>

extern void RotateMatrixU(TYPE_MAT *d, TYPE_MAT *s, S32 x, S32 y, S32 z);

/* ASM RotateMatrixU uses Watcom register convention:
   parm [edi] [esi] [eax] [ebx] [ecx], modify exact [eax ebx ecx edx esi] */
extern "C" void asm_RotateMatrixU(void);
static void call_asm_RotateMatrixU(TYPE_MAT *d, TYPE_MAT *s, S32 x, S32 y, S32 z) {
    __asm__ __volatile__(
        "call asm_RotateMatrixU"
        : "+a"(x), "+b"(y), "+c"(z), "+S"(s)
        : "D"(d)
        : "memory", "edx");
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static TYPE_MAT make_identity_mat(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 1.0f;
    m.F.M22 = 1.0f;
    m.F.M33 = 1.0f;
    return m;
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

static TYPE_MAT make_translated_test_mat(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 0.875f;
    m.F.M12 = -0.125f;
    m.F.M13 = 0.25f;
    m.F.M21 = 0.5f;
    m.F.M22 = 0.625f;
    m.F.M23 = -0.75f;
    m.F.M31 = -0.375f;
    m.F.M32 = 1.0f;
    m.F.M33 = 0.125f;
    m.F.TX = 48.0f;
    m.F.TY = -96.0f;
    m.F.TZ = 24.0f;
    return m;
}

static void fill_rand_mat(TYPE_MAT *m) {
    float *values = &m->F.M11;
    memset(m, 0, sizeof(*m));
    for (int index = 0; index < 9; ++index) {
        S32 sample = (S32)(rng_next() & 0x3FFu) - 512;
        values[index] = (float)sample / 256.0f;
    }
}

static void assert_rotmatu_case(const char *label, const TYPE_MAT *source_matrix, S32 x, S32 y, S32 z) {
    TYPE_MAT cpp_src;
    TYPE_MAT asm_src;
    TYPE_MAT cpp_dst;
    TYPE_MAT asm_dst;

    memcpy(&cpp_src, source_matrix, sizeof(cpp_src));
    memcpy(&asm_src, source_matrix, sizeof(asm_src));
    memset(&cpp_dst, 0xCC, sizeof(cpp_dst));
    memset(&asm_dst, 0xCC, sizeof(asm_dst));

    RotateMatrixU(&cpp_dst, &cpp_src, x, y, z);
    call_asm_RotateMatrixU(&asm_dst, &asm_src, x, y, z);

    ASSERT_ASM_CPP_MEM_EQ(&asm_dst, &cpp_dst, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(source_matrix, &cpp_src, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(source_matrix, &asm_src, sizeof(TYPE_MAT), label);
}

static void test_equivalence(void) {
    TYPE_MAT matrices[] = {
        make_identity_mat(),
        make_test_mat(),
        make_alt_test_mat(),
        make_translated_test_mat(),
    };
    struct {
        S32 a, b, g;
    } cases[] = {
        {0, 0, 0},
        {1024, 0, 0},
        {0, 1024, 0},
        {0, 0, 1024},
        {512, 512, 512},
        {1023, 1024, 1025},
        {-1, 0, 1},
        {4096, -4096, 8192},
        {2048, 2048, -2048},
    };
    for (int matrix_index = 0; matrix_index < (int)(sizeof(matrices) / sizeof(matrices[0])); ++matrix_index) {
        for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
            char lbl[128];
            snprintf(lbl, sizeof(lbl), "RotateMatrixU fixed m=%d a=%d b=%d g=%d",
                     matrix_index, cases[i].a, cases[i].b, cases[i].g);
            assert_rotmatu_case(lbl, &matrices[matrix_index], cases[i].a, cases[i].b, cases[i].g);
        }
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        TYPE_MAT src;
        S32 a = (S32)(rng_next() % 16384u) - 8192;
        S32 b = (S32)(rng_next() % 16384u) - 8192;
        S32 g = (S32)(rng_next() % 16384u) - 8192;
        char lbl[96];

        fill_rand_mat(&src);
        snprintf(lbl, sizeof(lbl), "RotateMatrixU rand a=%d b=%d g=%d", a, b, g);
        assert_rotmatu_case(lbl, &src, a, b, g);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
