/* Test: SinTabF / CosTabF — pre-computed float sine/cosine tables */
#include "test_harness.h"

#include <3D/SINTABF.H>
#include <math.h>

#ifdef LBA2_ASM_TESTS
extern "C" float *asm_SinTabF;
extern "C" float *asm_CosTabF;
#endif

static void test_sinf_zero(void)
{
    ASSERT_NEAR_F(0.0f, SinTabF[0], 0.001f);
}

static void test_cosf_zero(void)
{
    ASSERT_NEAR_F(1.0f, CosTabF[0], 0.001f);
}

static void test_sinf_90(void)
{
    ASSERT_NEAR_F(1.0f, SinTabF[1024], 0.001f);
}

static void test_cosf_90(void)
{
    ASSERT_NEAR_F(0.0f, CosTabF[1024], 0.001f);
}

static void test_sinf_matches_math(void)
{
    /* Spot-check a few values against C math library */
    for (int i = 0; i < 4096; i += 256) {
        double expected = sin(2.0 * M_PI * i / 4096.0);
        ASSERT_NEAR_F(expected, SinTabF[i], 0.002f);
    }
}

static void test_cosf_matches_math(void)
{
    for (int i = 0; i < 4096; i += 256) {
        double expected = cos(2.0 * M_PI * i / 4096.0);
        ASSERT_NEAR_F(expected, CosTabF[i], 0.002f);
    }
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    for (int i = 0; i < 4096; i++) {
        ASSERT_ASM_CPP_NEAR_F(asm_SinTabF[i], SinTabF[i], 0.0001f, "SinTabF");
    }
    for (int i = 0; i < 4096; i++) {
        ASSERT_ASM_CPP_NEAR_F(asm_CosTabF[i], CosTabF[i], 0.0001f, "CosTabF");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_sinf_zero);
    RUN_TEST(test_cosf_zero);
    RUN_TEST(test_sinf_90);
    RUN_TEST(test_cosf_90);
    RUN_TEST(test_sinf_matches_math);
    RUN_TEST(test_cosf_matches_math);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
