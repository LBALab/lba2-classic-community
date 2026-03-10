/* Test: SinTab / CosTab — pre-computed 16-bit sine/cosine tables */
#include "test_harness.h"

#include <3D/SINTAB.H>

#ifdef LBA2_ASM_TESTS
extern "C" S16 asm_SinTab[];
extern "C" S16 asm_CosTab[];
#endif

static void test_sin_zero(void)
{
    /* sin(0°) = 0 */
    ASSERT_EQ_INT(0, SinTab[0]);
}

static void test_cos_zero(void)
{
    /* cos(0°) = 16384 (1.0 in 2.14 fixed point) */
    ASSERT_EQ_INT(16384, CosTab[0]);
}

static void test_sin_90(void)
{
    /* sin(90°) at index 1024 = 16384 */
    ASSERT_EQ_INT(16384, SinTab[1024]);
}

static void test_cos_90(void)
{
    /* cos(90°) ≈ 0 */
    ASSERT_TRUE(CosTab[1024] >= -2 && CosTab[1024] <= 2);
}

static void test_sin_180(void)
{
    /* sin(180°) ≈ 0 */
    ASSERT_TRUE(SinTab[2048] >= -2 && SinTab[2048] <= 2);
}

static void test_cos_180(void)
{
    /* cos(180°) = -16384 */
    ASSERT_TRUE(CosTab[2048] <= -16380);
}

static void test_sin_symmetry(void)
{
    /* sin(x) = sin(4096-x) should be approx -sin(x) shifted... actually
       sin(4096-x) = -sin(x) for this table */
    for (int i = 1; i < 2048; i++) {
        S32 a = SinTab[i];
        S32 b = SinTab[4096 - i];
        /* They should be negatives of each other (with 4096 entries = 360°) */
        ASSERT_TRUE(a + b >= -2 && a + b <= 2);
    }
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    /* Compare all 4096+1024 entries (SinTab is 5120 entries to include CosTab offset) */
    for (int i = 0; i < 4096; i++) {
        ASSERT_ASM_CPP_EQ_INT(asm_SinTab[i], SinTab[i], "SinTab");
    }
    for (int i = 0; i < 4096; i++) {
        ASSERT_ASM_CPP_EQ_INT(asm_CosTab[i], CosTab[i], "CosTab");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_sin_zero);
    RUN_TEST(test_cos_zero);
    RUN_TEST(test_sin_90);
    RUN_TEST(test_cos_90);
    RUN_TEST(test_sin_180);
    RUN_TEST(test_cos_180);
    RUN_TEST(test_sin_symmetry);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
