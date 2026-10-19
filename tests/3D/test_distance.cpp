/* Test: Distance2D / Distance3D - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/DISTANCE.H>

/* ASM Distance2D uses Watcom register convention:
   parm [eax] [ecx] [ebx] [edx], result in eax. It jumps to QSqr, which also
   uses ebx, ecx, edx, esi and edi. */
extern "C" void asm_Distance2D(void);
static U32 call_asm_Distance2D(S32 x0, S32 y0, S32 x1, S32 y1) {
    U32 result;
    __asm__ __volatile__("call asm_Distance2D"
                         : "=a"(result), "+c"(y0), "+b"(x1), "+d"(y1)
                         : "a"(x0)
                         : "memory", "esi", "edi");
    return result;
}

/* ASM Distance3D: parm [eax] [ecx] [ebx] [edx] [esi] [edi], result in eax. */
extern "C" void asm_Distance3D(void);
static U32 call_asm_Distance3D(S32 x0, S32 y0, S32 z0, S32 x1, S32 y1, S32 z1) {
    U32 result;
    __asm__ __volatile__("call asm_Distance3D"
                         : "=a"(result), "+c"(y0), "+b"(z0), "+d"(x1), "+S"(y1), "+D"(z1)
                         : "a"(x0)
                         : "memory");
    return result;
}

static void assert_2d(const char *what, S32 x0, S32 y0, S32 x1, S32 y1) {
    char label[128];
    snprintf(label, sizeof label, "Distance2D %s (%d,%d)-(%d,%d)", what, x0, y0, x1, y1);
    ASSERT_ASM_CPP_EQ_INT(call_asm_Distance2D(x0, y0, x1, y1), Distance2D(x0, y0, x1, y1), label);
}

static void assert_3d(const char *what, S32 x0, S32 y0, S32 z0, S32 x1, S32 y1, S32 z1) {
    char label[160];
    snprintf(label, sizeof label, "Distance3D %s (%d,%d,%d)-(%d,%d,%d)", what, x0, y0, z0, x1, y1,
             z1);
    ASSERT_ASM_CPP_EQ_INT(call_asm_Distance3D(x0, y0, z0, x1, y1, z1),
                          Distance3D(x0, y0, z0, x1, y1, z1), label);
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static S32 rng_coord(U32 bits) {
    U32 v = (rng_next() << 17) ^ (rng_next() << 2) ^ (rng_next() & 0x3u);
    return (S32)v >> (32 - bits);
}

static void test_fixed(void) {
    assert_2d("zero", 0, 0, 0, 0);
    assert_2d("3-4-5", 0, 0, 3, 4);
    assert_2d("last before 32-bit overflow", 0, 0, 46340, 0);
    assert_2d("first past 32-bit overflow", 0, 0, 46341, 0);
    assert_2d("corners of S16", 32767, 32767, -32768, -32768);
    assert_2d("one below a square", 0, 0, 65535, 1);
    assert_2d("delta wraps", 0x7FFFFFFF, 0, -1, 0);
    assert_2d("largest delta", (S32)0x80000000, (S32)0x80000000, 0, 0);

    assert_3d("zero", 0, 0, 0, 0, 0, 0);
    assert_3d("3-4-12", 0, 0, 0, 3, 4, 12);
    assert_3d("last before 32-bit overflow", 0, 0, 0, 26754, 26754, 26754);
    assert_3d("first past 32-bit overflow", 0, 0, 0, 26756, 26756, 26756);
    assert_3d("plausible small answer before", 0, 0, 0, 40000, 40000, 40000);
    assert_3d("delta wraps", 0x7FFFFFFF, 0, 0, -1, 0, 0);
    assert_3d("largest sum of squares", (S32)0x80000000, (S32)0x80000000, (S32)0x80000000, 0, 0,
              0);
}

static void test_random(void) {
    /* Game-sized coordinates (17 bits), then the full S32 range. */
    static const U32 widths[] = {17, 20, 32};
    rng_seed(0x600u);
    for (int w = 0; w < (int)(sizeof widths / sizeof widths[0]); w++) {
        for (int i = 0; i < 300; i++) {
            U32 b = widths[w];
            assert_2d("rand", rng_coord(b), rng_coord(b), rng_coord(b), rng_coord(b));
            assert_3d("rand", rng_coord(b), rng_coord(b), rng_coord(b), rng_coord(b), rng_coord(b),
                      rng_coord(b));
        }
    }
}

int main(void) {
    RUN_TEST(test_fixed);
    RUN_TEST(test_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
