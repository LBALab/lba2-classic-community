/* Test: LongInverseRotatePointI - ASM vs CPP equivalence (the 1997 integer path) */
#include "test_harness.h"
#include <3D/LIROT3D.H>
#include <3D/IMATSTD.H>
#include <3D/CAMERA.H>
#include <string.h>

extern "C" void asm_LongInverseRotatePointI(void);
static void call_asm_LongInverseRotatePointI(TYPE_MAT *m, S32 x, S32 y, S32 z) {
    /* Watcom: parm [esi] [eax] [ebx] [ecx], modify exact [eax ebx ecx edx edi]. */
    __asm__ __volatile__("call asm_LongInverseRotatePointI"
                         : "+a"(x), "+b"(y), "+c"(z) : "S"(m) : "memory", "edx", "edi");
}

static U32 rng_state;

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state;
}

static void assert_rotate_case(const char *label, const TYPE_MAT *source, S32 x, S32 y, S32 z) {
    TYPE_MAT cpp_matrix;
    TYPE_MAT asm_matrix;
    memcpy(&cpp_matrix, source, sizeof(cpp_matrix));
    memcpy(&asm_matrix, source, sizeof(asm_matrix));

    X0 = Y0 = Z0 = 0x13579BDF;
    LongInverseRotatePointI(&cpp_matrix, x, y, z);
    S32 cpp_x0 = X0, cpp_y0 = Y0, cpp_z0 = Z0;

    X0 = Y0 = Z0 = 0x2468ACE0;
    call_asm_LongInverseRotatePointI(&asm_matrix, x, y, z);

    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, label);
    ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, label);
    ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z0, label);
    ASSERT_ASM_CPP_MEM_EQ(source, &cpp_matrix, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(source, &asm_matrix, sizeof(TYPE_MAT), label);
}

static TYPE_MAT camera_matrix(S32 a, S32 b, S32 g) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    InitMatrixStdI(&m, a, b, g);
    return m;
}

/* Camera matrices on camera-sized coordinates: what SetFollowCamera feeds it. */
static void test_camera_matrices(void) {
    const S32 angles[][3] = {{0, 0, 0}, {300, 4055, 0}, {300, 1024, 0}, {450, 2500, 0}, {3800, 700, 100}, {1024, 3072, 2048}};
    const S32 points[][3] = {
        {0, 0, 0},
        {512, 0, 512},
        {32768, -50, 32768},
        {-32768, 12500, 65536},
        {28300, 3370, 9700},
        {98304, 0, -98304},
        {1, -1, 1},
        {-1200, 800, 27000}, /* a rotated camera backed out by camzoom */
    };
    char lbl[128];
    for (int a = 0; a < (int)(sizeof(angles) / sizeof(angles[0])); a++) {
        TYPE_MAT m = camera_matrix(angles[a][0], angles[a][1], angles[a][2]);
        for (int p = 0; p < (int)(sizeof(points) / sizeof(points[0])); p++) {
            snprintf(lbl, sizeof(lbl), "LongInverseRotatePointI cam=%d x=%d y=%d z=%d", a, points[p][0], points[p][1], points[p][2]);
            assert_rotate_case(lbl, &m, points[p][0], points[p][1], points[p][2]);
        }
    }
}

/* Sums that land on and either side of the rounding bit, positive and negative. */
static void test_rounding_edges(void) {
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.I.M11 = 1;
    m.I.M22 = -1;
    m.I.M33 = 3;
    const S32 values[] = {8191, 8192, 8193, 16383, 16384, 24576, -8191, -8192, -8193, -24576, 0x7FFFFFFF, (S32)0x80000000};
    char lbl[96];
    for (int i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++) {
        snprintf(lbl, sizeof(lbl), "LongInverseRotatePointI edge v=%d", values[i]);
        assert_rotate_case(lbl, &m, values[i], values[i], values[i]);
    }
}

/* Any 2.14 matrix and any 32-bit point: the 64-bit sums and the 32-bit wrap. */
static void test_random(void) {
    rng_state = 0xC0FFEE02u;
    char lbl[128];
    for (int i = 0; i < 100000; i++) {
        TYPE_MAT m;
        memset(&m, 0, sizeof(m));
        S32 *v = &m.I.M11;
        for (int k = 0; k < 9; k++)
            v[k] = (S32)(rng_next() % 32769u) - 16384;
        S32 x, y, z;
        if (i & 1) {
            x = (S32)rng_next();
            y = (S32)rng_next();
            z = (S32)rng_next();
        } else {
            x = (S32)(rng_next() % 262144u) - 131072;
            y = (S32)(rng_next() % 65536u) - 32768;
            z = (S32)(rng_next() % 262144u) - 131072;
        }
        snprintf(lbl, sizeof(lbl), "LongInverseRotatePointI rand i=%d x=%d y=%d z=%d", i, x, y, z);
        assert_rotate_case(lbl, &m, x, y, z);
    }
}

int main(void) {
    RUN_TEST(test_camera_matrices);
    RUN_TEST(test_rounding_edges);
    RUN_TEST(test_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
