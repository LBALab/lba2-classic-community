/* Test: MulMatrixF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/MULMAT.H>
#include <string.h>

extern void MulMatrixF(TYPE_MAT *d, TYPE_MAT *s1, TYPE_MAT *s2);

extern "C" void asm_MulMatrixF(void);
static void call_asm_MulMatrixF(TYPE_MAT *d, TYPE_MAT *s1, TYPE_MAT *s2) {
    __asm__ __volatile__("call asm_MulMatrixF" : : "D"(d), "S"(s1), "b"(s2) : "memory", "eax");
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void make_rand_mat(TYPE_MAT *m) {
    float *f = &m->F.M11;
    memset(m, 0, sizeof(*m));
    for (int j = 0; j < 9; j++) {
        S32 sample = (S32)(rng_next() & 0x3FFu) - 512;
        f[j] = (float)sample / 256.0f;
    }
}

static TYPE_MAT make_identity_mat(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 1.0f;
    m.F.M22 = 1.0f;
    m.F.M33 = 1.0f;
    return m;
}

static TYPE_MAT make_test_mat_a(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 1.5f;
    m.F.M12 = 2.3f;
    m.F.M13 = 0.7f;
    m.F.M21 = 0.1f;
    m.F.M22 = 3.2f;
    m.F.M23 = 1.1f;
    m.F.M31 = 2.0f;
    m.F.M32 = 0.5f;
    m.F.M33 = 4.0f;
    return m;
}

static TYPE_MAT make_test_mat_b(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 0.9f;
    m.F.M12 = 1.2f;
    m.F.M13 = 0.3f;
    m.F.M21 = 2.1f;
    m.F.M22 = 0.4f;
    m.F.M23 = 1.5f;
    m.F.M31 = 0.6f;
    m.F.M32 = 3.0f;
    m.F.M33 = 0.8f;
    return m;
}

static void assert_mul_case(const char *label, const TYPE_MAT *src1, const TYPE_MAT *src2) {
    TYPE_MAT cpp_src1;
    TYPE_MAT cpp_src2;
    TYPE_MAT asm_src1;
    TYPE_MAT asm_src2;
    TYPE_MAT cpp_dst;
    TYPE_MAT asm_dst;

    memcpy(&cpp_src1, src1, sizeof(cpp_src1));
    memcpy(&cpp_src2, src2, sizeof(cpp_src2));
    memcpy(&asm_src1, src1, sizeof(asm_src1));
    memcpy(&asm_src2, src2, sizeof(asm_src2));
    memset(&cpp_dst, 0xCC, sizeof(cpp_dst));
    memset(&asm_dst, 0xCC, sizeof(asm_dst));

    MulMatrixF(&cpp_dst, &cpp_src1, &cpp_src2);
    call_asm_MulMatrixF(&asm_dst, &asm_src1, &asm_src2);

    ASSERT_ASM_CPP_MEM_EQ(&asm_dst, &cpp_dst, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(src1, &cpp_src1, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(src2, &cpp_src2, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(src1, &asm_src1, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(src2, &asm_src2, sizeof(TYPE_MAT), label);
}

static void test_equivalence(void) {
    TYPE_MAT identity = make_identity_mat();
    TYPE_MAT a = make_test_mat_a();
    TYPE_MAT b = make_test_mat_b();

    assert_mul_case("MulMatrixF identity-left", &identity, &a);
    assert_mul_case("MulMatrixF identity-right", &a, &identity);
    assert_mul_case("MulMatrixF fixed", &a, &b);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        TYPE_MAT a;
        TYPE_MAT b;
        char lbl[64];

        make_rand_mat(&a);
        make_rand_mat(&b);
        snprintf(lbl, sizeof(lbl), "MulMatrixF rand %d", i);
        assert_mul_case(lbl, &a, &b);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
