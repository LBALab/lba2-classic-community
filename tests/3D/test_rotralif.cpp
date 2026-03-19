/* Test: RotTransListF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROTRALIS.H>
#include <string.h>

extern void RotTransListF(TYPE_MAT *m, TYPE_VT16 *d, TYPE_VT16 *s, S32 n);
extern "C" void asm_RotTransListF(void);
static void call_asm_RotTransListF(TYPE_MAT *m, TYPE_VT16 *d, TYPE_VT16 *s, S32 n) {
    __asm__ __volatile__("call asm_RotTransListF"
        : "+c"(n), "+S"(s), "+D"(d) : "b"(m) : "memory");
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

static TYPE_MAT make_test_mat(void)
{
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = 0.5f; m.F.M12 = 0.3f; m.F.M13 = 0.1f;
    m.F.M21 = 0.2f; m.F.M22 = 0.8f; m.F.M23 = 0.4f;
    m.F.M31 = 0.7f; m.F.M32 = 0.6f; m.F.M33 = 0.9f;
    m.F.TX = 5.0f; m.F.TY = 10.0f; m.F.TZ = 15.0f;
    return m;
}

static TYPE_MAT make_alt_test_mat(void)
{
    TYPE_MAT m;
    memset(&m, 0, sizeof(m));
    m.F.M11 = -0.75f; m.F.M12 = 0.25f; m.F.M13 = 0.5f;
    m.F.M21 = 0.125f; m.F.M22 = -0.5f; m.F.M23 = 0.75f;
    m.F.M31 = 0.625f; m.F.M32 = -0.25f; m.F.M33 = 0.375f;
    m.F.TX = -24.0f; m.F.TY = 63.0f; m.F.TZ = -11.0f;
    return m;
}

static void fill_rand_mat(TYPE_MAT *m)
{
    float *values = &m->F.M11;
    memset(m, 0, sizeof(*m));
    for (int index = 0; index < 9; ++index) {
        S32 sample = (S32)(rng_next() & 0x3FFu) - 512;
        values[index] = (float)sample / 256.0f;
    }
    m->F.TX = (float)((S32)(rng_next() & 0x1FFu) - 256);
    m->F.TY = (float)((S32)(rng_next() & 0x1FFu) - 256);
    m->F.TZ = (float)((S32)(rng_next() & 0x1FFu) - 256);
}

static TYPE_VT16 make_point(S16 x, S16 y, S16 z, S16 grp)
{
    TYPE_VT16 point;
    point.X = x;
    point.Y = y;
    point.Z = z;
    point.Grp = grp;
    return point;
}

static void fill_rand_points(TYPE_VT16 *points, S32 count)
{
    for (S32 index = 0; index < count; ++index) {
        points[index].X = (S16)((S32)(rng_next() & 0x7FFu) - 1024);
        points[index].Y = (S16)((S32)(rng_next() & 0x7FFu) - 1024);
        points[index].Z = (S16)((S32)(rng_next() & 0x7FFu) - 1024);
        points[index].Grp = (S16)(rng_next() & 0x7FFFu);
    }
}

static void assert_rotralif_case(const char *label, const TYPE_MAT *matrix, const TYPE_VT16 *source, S32 count)
{
    TYPE_MAT cpp_mat;
    TYPE_MAT asm_mat;
    TYPE_VT16 cpp_src[8];
    TYPE_VT16 asm_src[8];
    TYPE_VT16 cpp_dst[8];
    TYPE_VT16 asm_dst[8];

    memcpy(&cpp_mat, matrix, sizeof(cpp_mat));
    memcpy(&asm_mat, matrix, sizeof(asm_mat));
    memcpy(cpp_src, source, (size_t)count * sizeof(TYPE_VT16));
    memcpy(asm_src, source, (size_t)count * sizeof(TYPE_VT16));
    memset(cpp_dst, 0xCC, sizeof(cpp_dst));
    memset(asm_dst, 0xCC, sizeof(asm_dst));

    RotTransListF(&cpp_mat, cpp_dst, cpp_src, count);
    call_asm_RotTransListF(&asm_mat, asm_dst, asm_src, count);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, (size_t)count * sizeof(TYPE_VT16), label);
    ASSERT_ASM_CPP_MEM_EQ(matrix, &cpp_mat, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(matrix, &asm_mat, sizeof(TYPE_MAT), label);
    ASSERT_ASM_CPP_MEM_EQ(source, cpp_src, (size_t)count * sizeof(TYPE_VT16), label);
    ASSERT_ASM_CPP_MEM_EQ(source, asm_src, (size_t)count * sizeof(TYPE_VT16), label);
}

static void test_equivalence(void)
{
    TYPE_MAT matrices[] = {
        make_test_mat(),
        make_alt_test_mat(),
    };
    TYPE_VT16 points[] = {
        make_point(100, 200, 300, 1),
        make_point(-50, 0, 50, 2),
        make_point(0, 0, 0, 3),
        make_point(32767, -32768, 1234, 4),
        make_point(-1024, 2048, -4096, 5),
    };

    assert_rotralif_case("RotTransListF fixed n=1", &matrices[0], points, 1);
    assert_rotralif_case("RotTransListF fixed n=3", &matrices[0], points, 3);
    assert_rotralif_case("RotTransListF fixed n=5", &matrices[0], points, 5);
    assert_rotralif_case("RotTransListF alt n=4", &matrices[1], points, 4);
}

static void test_random_equivalence(void)
{
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; ++i) {
        TYPE_MAT matrix;
        TYPE_VT16 points[8];
        S32 count = (S32)(rng_next() % 8u) + 1;
        char label[64];

        fill_rand_mat(&matrix);
        fill_rand_points(points, count);
        snprintf(label, sizeof(label), "RotTransListF rand %d", i);
        assert_rotralif_case(label, &matrix, points, count);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
