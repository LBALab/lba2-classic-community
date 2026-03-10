/* Test: GetAngleVector2D — 2D angle from (x,z) via tangent binary search */
#include "test_harness.h"

#include <3D/GETANG2D.H>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_GetAngleVector2D(S32 x, S32 z);
#endif

/* ── CPP correctness tests ─────────────────────────────────────────────── */

static void test_origin(void)
{
    /* x=0,z>0 → angle 0 (north) */
    ASSERT_EQ_INT(0, GetAngleVector2D(0, 1));
    ASSERT_EQ_INT(0, GetAngleVector2D(0, 1000));
}

static void test_x_positive_z_zero(void)
{
    /* Pure east → 1024 (90°) */
    ASSERT_EQ_INT(1024, GetAngleVector2D(1, 0));
    ASSERT_EQ_INT(1024, GetAngleVector2D(999, 0));
}

static void test_x_negative_z_zero(void)
{
    /* Pure west → 3072 (270°) */
    ASSERT_EQ_INT(4096 - 1024, GetAngleVector2D(-1, 0));
    ASSERT_EQ_INT(4096 - 1024, GetAngleVector2D(-999, 0));
}

static void test_z_negative(void)
{
    /* x=0,z<0 → 2048 (180°) */
    ASSERT_EQ_INT(2048, GetAngleVector2D(0, -1));
    ASSERT_EQ_INT(2048, GetAngleVector2D(0, -1000));
}

static void test_diagonal(void)
{
    /* 45° → ~512 */
    S32 a = GetAngleVector2D(100, 100);
    ASSERT_TRUE(a >= 500 && a <= 524);
}

static void test_symmetry(void)
{
    /* angle(x,z) and angle(-x,z) should sum to 4096 mod 4096 */
    S32 a1 = GetAngleVector2D(100, 200);
    S32 a2 = GetAngleVector2D(-100, 200);
    ASSERT_EQ_INT(0, (a1 + a2) & 4095);
}

static void test_large_values(void)
{
    S32 a = GetAngleVector2D(100000, 100000);
    ASSERT_TRUE(a >= 500 && a <= 524);
}

/* ── ASM vs CPP equivalence ─────────────────────────────────────────── */
#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    struct { S32 x, z; } cases[] = {
        {0, 0}, {0, 1}, {1, 0}, {0, -1}, {-1, 0},
        {100, 100}, {-100, 100}, {100, -100}, {-100, -100},
        {1, 1000}, {1000, 1}, {-500, 300}, {300, -500},
        {100000, 1}, {1, 100000}, {-100000, -100000},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        S32 cpp = GetAngleVector2D(cases[i].x, cases[i].z);
        S32 asm_r = asm_GetAngleVector2D(cases[i].x, cases[i].z);
        ASSERT_ASM_CPP_EQ_INT(asm_r, cpp, "GetAngleVector2D");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_origin);
    RUN_TEST(test_x_positive_z_zero);
    RUN_TEST(test_x_negative_z_zero);
    RUN_TEST(test_z_negative);
    RUN_TEST(test_diagonal);
    RUN_TEST(test_symmetry);
    RUN_TEST(test_large_values);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
