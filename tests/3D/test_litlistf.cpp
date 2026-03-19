/* Test: LightList - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LITLISTF.H>
#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>
#include <string.h>

/* ASM LightListF uses Watcom register convention:
   parm [ebx] [edi] [esi] [ecx], modify [eax ebx edx] */
extern "C" void asm_LightListF(void);
static void call_asm_LightListF(TYPE_MAT *m, U16 *d, TYPE_VT16 *s, S32 n) {
    __asm__ __volatile__(
        "call asm_LightListF"
        : "+b"(m), "+D"(d), "+S"(s), "+c"(n)
        :
        : "memory", "eax", "edx");
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void set_identity_matrix(TYPE_MAT *m) {
    memset(m, 0, sizeof(*m));
    m->F.M11 = 1.0f;
    m->F.M22 = 1.0f;
    m->F.M33 = 1.0f;
}

static void assert_lightlist_case(const char *label, TYPE_MAT *matrix, TYPE_VT16 *src, S32 n) {
    TYPE_MAT cpp_matrix;
    TYPE_MAT asm_matrix;
    U16 cpp_dst[8];
    U16 asm_dst[8];

    ASSERT_TRUE(n >= 0 && n <= 8);

    memcpy(&cpp_matrix, matrix, sizeof(cpp_matrix));
    memcpy(&asm_matrix, matrix, sizeof(asm_matrix));
    memset(cpp_dst, 0xCC, sizeof(cpp_dst));
    memset(asm_dst, 0xCC, sizeof(asm_dst));

    LightList(&cpp_matrix, cpp_dst, src, n);
    S32 cpp_x0 = X0;
    S32 cpp_y0 = Y0;
    S32 cpp_z0 = Z0;

    call_asm_LightListF(&asm_matrix, asm_dst, src, n);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, (size_t)n * sizeof(U16), label);
    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, label);
    ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, label);
    ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z0, label);
    ASSERT_ASM_CPP_MEM_EQ(&asm_matrix, &cpp_matrix, sizeof(TYPE_MAT), label);
}

static void test_equivalence(void) {
    TYPE_MAT matrix;
    TYPE_VT16 src[3] = {{100, 0, 0, 0}, {0, 100, 0, 0}, {0, 0, 100, 0}};

    set_identity_matrix(&matrix);
    CameraXLight = 50;
    CameraYLight = 50;
    CameraZLight = 50;
    FactorLight = 0.01f;

    assert_lightlist_case("LightList fixed", &matrix, src, 3);
}

static void test_random_equivalence(void) {
    TYPE_MAT matrix;
    rng_seed(0xDEADBEEFu);
    set_identity_matrix(&matrix);

    for (int i = 0; i < 200; i++) {
        TYPE_VT16 src[4];
        S32 n = (S32)(rng_next() % 4u) + 1;

        FactorLight = ((float)((S32)(rng_next() % 33u) - 16)) / 256.0f;
        CameraXLight = (S32)(rng_next() % 257u) - 128;
        CameraYLight = (S32)(rng_next() % 257u) - 128;
        CameraZLight = (S32)(rng_next() % 257u) - 128;

        for (S32 index = 0; index < n; ++index) {
            src[index].X = (S16)((S32)(rng_next() % 513u) - 256);
            src[index].Y = (S16)((S32)(rng_next() % 513u) - 256);
            src[index].Z = (S16)((S32)(rng_next() % 513u) - 256);
            src[index].Grp = 0;
        }

        char lbl[128];
        snprintf(lbl, sizeof(lbl), "LightList rand n=%d cx=%d cy=%d cz=%d", n, CameraXLight, CameraYLight, CameraZLight);
        assert_lightlist_case(lbl, &matrix, src, n);
    }
}

static void test_zero_count_preserves_globals(void) {
    TYPE_MAT matrix;
    TYPE_VT16 src[1] = {{1, 2, 3, 0}};

    set_identity_matrix(&matrix);
    CameraXLight = 7;
    CameraYLight = -11;
    CameraZLight = 13;
    FactorLight = 0.125f;
    X0 = 111;
    Y0 = 222;
    Z0 = 333;
    LightList(&matrix, NULL, src, 0);
    S32 cpp_x0 = X0;
    S32 cpp_y0 = Y0;
    S32 cpp_z0 = Z0;

    X0 = 111;
    Y0 = 222;
    Z0 = 333;
    call_asm_LightListF(&matrix, NULL, src, 0);

    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, "LightList zero-count X0");
    ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, "LightList zero-count Y0");
    ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z0, "LightList zero-count Z0");
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_zero_count_preserves_globals);
    TEST_SUMMARY();
    return test_failures != 0;
}
