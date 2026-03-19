/* Test: InitMatrixTransF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/IMATTRA.H>
#include <string.h>

extern void InitMatrixTransF(TYPE_MAT *m, S32 tx, S32 ty, S32 tz);

extern "C" void asm_InitMatrixTransF(void);
static void call_asm_InitMatrixTransF(TYPE_MAT *m, S32 tx, S32 ty, S32 tz) {
    __asm__ __volatile__("call asm_InitMatrixTransF" : : "D"(m), "a"(tx), "b"(ty), "c"(tz) : "memory");
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static U32 float_bits(float value) {
    U32 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void initmatrixtrans_buggy_integer_overlay(TYPE_MAT *m, S32 tx, S32 ty, S32 tz) {
    memset(m, 0, sizeof(*m));
    m->I.TX = tx;
    m->I.TY = ty;
    m->I.TZ = tz;
}

static void init_matrix_pattern(TYPE_MAT *m) {
    memset(m, 0xA5, sizeof(*m));
}

static void assert_translation_case(const char *label, S32 tx, S32 ty, S32 tz, int zero_init) {
    TYPE_MAT cpp_matrix;
    TYPE_MAT asm_matrix;

    if (zero_init) {
        memset(&cpp_matrix, 0, sizeof(cpp_matrix));
        memset(&asm_matrix, 0, sizeof(asm_matrix));
    } else {
        init_matrix_pattern(&cpp_matrix);
        init_matrix_pattern(&asm_matrix);
    }

    InitMatrixTransF(&cpp_matrix, tx, ty, tz);
    call_asm_InitMatrixTransF(&asm_matrix, tx, ty, tz);

    ASSERT_ASM_CPP_MEM_EQ(&asm_matrix, &cpp_matrix, sizeof(TYPE_MAT), label);
}

static void test_equivalence(void) {
    struct {
        S32 tx, ty, tz;
    } cases[] = {
        {0, 0, 0},
        {1, 2, 3},
        {100, 200, 300},
        {-500, -1000, -2000},
        {2147483647, -2147483647, 0},
        {-1, 1, -1},
        {32767, -32768, 123456789},
    };
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "InitMatrixTransF fixed tx=%d ty=%d tz=%d", cases[i].tx, cases[i].ty, cases[i].tz);
        assert_translation_case(lbl, cases[i].tx, cases[i].ty, cases[i].tz, 1);
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        S32 tx = ((S32)rng_next() << 1) - 0x4000;
        S32 ty = ((S32)rng_next() << 1) - 0x4000;
        S32 tz = ((S32)rng_next() << 1) - 0x4000;
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "InitMatrixTransF rand tx=%d ty=%d tz=%d", tx, ty, tz);
        assert_translation_case(lbl, tx, ty, tz, 1);
    }
}

static void test_translation_slots_use_float_representation(void) {
    struct {
        S32 tx, ty, tz;
    } cases[] = {
        {1, -2, 1024},
        {7, 255, -4096},
        {-1234567, 7654321, 32768},
    };

    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        TYPE_MAT cm;
        TYPE_MAT am;
        TYPE_MAT buggy;

        memset(&cm, 0xCC, sizeof(cm));
        memset(&am, 0xCC, sizeof(am));

        InitMatrixTransF(&cm, cases[i].tx, cases[i].ty, cases[i].tz);
        call_asm_InitMatrixTransF(&am, cases[i].tx, cases[i].ty, cases[i].tz);
        initmatrixtrans_buggy_integer_overlay(&buggy, cases[i].tx, cases[i].ty, cases[i].tz);

        ASSERT_ASM_CPP_MEM_EQ(&am, &cm, sizeof(TYPE_MAT), "InitMatrixTransF float translation slots");
        ASSERT_EQ_UINT(float_bits((float)cases[i].tx), (U32)cm.I.TX);
        ASSERT_EQ_UINT(float_bits((float)cases[i].ty), (U32)cm.I.TY);
        ASSERT_EQ_UINT(float_bits((float)cases[i].tz), (U32)cm.I.TZ);
        ASSERT_EQ_UINT((U32)cm.I.TX, (U32)am.I.TX);
        ASSERT_EQ_UINT((U32)cm.I.TY, (U32)am.I.TY);
        ASSERT_EQ_UINT((U32)cm.I.TZ, (U32)am.I.TZ);
        ASSERT_TRUE((U32)buggy.I.TX != (U32)cm.I.TX);
        ASSERT_TRUE((U32)buggy.I.TY != (U32)cm.I.TY);
        ASSERT_TRUE((U32)buggy.I.TZ != (U32)cm.I.TZ);
    }
}

static void test_non_translation_fields_preserved(void) {
    struct {
        S32 tx, ty, tz;
    } cases[] = {
        {5, -7, 9},
        {-1024, 2048, -4096},
        {123456, -654321, 42},
    };

    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "InitMatrixTransF preserve tx=%d ty=%d tz=%d", cases[i].tx, cases[i].ty, cases[i].tz);
        assert_translation_case(lbl, cases[i].tx, cases[i].ty, cases[i].tz, 0);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_translation_slots_use_float_representation);
    RUN_TEST(test_non_translation_fields_preserved);
    TEST_SUMMARY();
    return test_failures != 0;
}
