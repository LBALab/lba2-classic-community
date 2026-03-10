/* Test: Sqr (32-bit sqrt) and QSqr (64-bit sqrt) */
#include "test_harness.h"

#include <3D/SQRROOT.H>

#ifdef LBA2_ASM_TESTS
extern "C" U32 asm_Sqr(U32 x);
extern "C" U32 asm_QSqr(U32 xLow, U32 xHigh);
#endif

static void test_sqr_zero(void)
{
    ASSERT_EQ_UINT(0, Sqr(0));
}

static void test_sqr_one(void)
{
    ASSERT_EQ_UINT(1, Sqr(1));
}

static void test_sqr_perfect_squares(void)
{
    ASSERT_EQ_UINT(2, Sqr(4));
    ASSERT_EQ_UINT(3, Sqr(9));
    ASSERT_EQ_UINT(10, Sqr(100));
    ASSERT_EQ_UINT(100, Sqr(10000));
    ASSERT_EQ_UINT(1000, Sqr(1000000));
}

static void test_sqr_non_perfect(void)
{
    /* sqrt(2) ≈ 1.41, truncated to 1 */
    ASSERT_EQ_UINT(1, Sqr(2));
    /* sqrt(3) ≈ 1.73, truncated to 1 */
    ASSERT_EQ_UINT(1, Sqr(3));
    /* sqrt(5) ≈ 2.23, truncated to 2 */
    ASSERT_EQ_UINT(2, Sqr(5));
}

static void test_sqr_large(void)
{
    /* sqrt(0xFFFFFFFF) = sqrt(4294967295) ≈ 65535.99 */
    ASSERT_EQ_UINT(65535, Sqr(0xFFFFFFFF));
}

static void test_qsqr_small(void)
{
    /* QSqr with high=0 behaves like Sqr */
    ASSERT_EQ_UINT(0, QSqr(0, 0));
    ASSERT_EQ_UINT(1, QSqr(1, 0));
    ASSERT_EQ_UINT(10, QSqr(100, 0));
}

static void test_qsqr_64bit(void)
{
    /* sqrt(2^32) = 65536 */
    ASSERT_EQ_UINT(65536, QSqr(0, 1));
    /* sqrt(2^33) ≈ 92681 */
    U32 r = QSqr(0, 2);
    ASSERT_TRUE(r >= 92681 && r <= 92682);
}

#ifdef LBA2_ASM_TESTS
static void test_sqr_asm_equiv(void)
{
    U32 cases[] = {0, 1, 2, 3, 4, 9, 100, 10000, 1000000, 0xFFFFFFFF};
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        U32 cpp = Sqr(cases[i]);
        U32 asm_r = asm_Sqr(cases[i]);
        ASSERT_ASM_CPP_EQ_INT(asm_r, cpp, "Sqr");
    }
}

static void test_qsqr_asm_equiv(void)
{
    struct { U32 lo, hi; } cases[] = {
        {0, 0}, {1, 0}, {100, 0}, {0, 1}, {0, 2},
        {0xFFFFFFFF, 0}, {0, 0xFFFFFFFF},
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        U32 cpp = QSqr(cases[i].lo, cases[i].hi);
        U32 asm_r = asm_QSqr(cases[i].lo, cases[i].hi);
        /* Known: QSqr can differ by ±1 for large 64-bit values */
        S32 diff = (S32)asm_r - (S32)cpp;
        if (diff < -1 || diff > 1) {
            FAIL_MSG("[QSqr] ASM=%u vs CPP=%u (diff %d > 1)", asm_r, cpp, diff);
        }
    }
}
#endif

int main(void)
{
    RUN_TEST(test_sqr_zero);
    RUN_TEST(test_sqr_one);
    RUN_TEST(test_sqr_perfect_squares);
    RUN_TEST(test_sqr_non_perfect);
    RUN_TEST(test_sqr_large);
    RUN_TEST(test_qsqr_small);
    RUN_TEST(test_qsqr_64bit);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_sqr_asm_equiv);
    RUN_TEST(test_qsqr_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
