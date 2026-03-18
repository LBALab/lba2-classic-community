/* Test: LongProjectPoint3D - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/PROJ.H>
#include <3D/CAMERA.H>

extern S32 LongProjectPoint3D(S32 x, S32 y, S32 z);
extern "C" void asm_LongProjectPoint3DF(void);
static S32 call_asm_LongProjectPoint3DF(S32 x, S32 y, S32 z) {
    S32 result;
    __asm__ __volatile__("call asm_LongProjectPoint3DF"
        : "=a"(result) : "0"(x), "b"(y), "c"(z) : "memory", "edx");
    return result;
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

static void assert_projection_case(const char *label, S32 x, S32 y, S32 z)
{
    S32 cpp_ret = LongProjectPoint3D(x, y, z);
    S32 cpp_xp = Xp;
    S32 cpp_yp = Yp;
    S32 asm_ret = call_asm_LongProjectPoint3DF(x, y, z);

    ASSERT_ASM_CPP_EQ_INT(asm_ret, cpp_ret, label);
    ASSERT_ASM_CPP_EQ_INT(Xp, cpp_xp, label);
    ASSERT_ASM_CPP_EQ_INT(Yp, cpp_yp, label);
}

static void configure_projection_state(S32 xcentre, S32 ycentre, S32 near_clip, S32 ratio_x, S32 ratio_y,
                                       S32 camera_xr, S32 camera_yr, S32 camera_zr)
{
    SetProjection(xcentre, ycentre, near_clip, ratio_x, ratio_y);
    CameraXr = camera_xr;
    CameraYr = camera_yr;
    CameraZr = camera_zr;
    CameraZrClip = CameraZr - NearClip;
}

static void test_equivalence(void)
{
    struct {
        S32 xcentre, ycentre, near_clip, ratio_x, ratio_y;
        S32 camera_xr, camera_yr, camera_zr;
    } configs[] = {
        {320, 240, 1, 300, 300, 0, 0, 1000},
        {160, 100, 8, 256, 192, -50, 25, 750},
        {400, 300, 32, 512, 384, 100, -200, 2000},
    };
    struct { S32 x, y, z; } cases[] = {
        {0, 0, 0},
        {100, 0, 0},
        {0, 100, 0},
        {-100, -100, 0},
        {0, 0, 500},
        {0, 0, 2000},
        {1024, -512, 999},
        {-2048, 1536, 1200},
    };

    for (int config_index = 0; config_index < (int)(sizeof(configs) / sizeof(configs[0])); ++config_index) {
        configure_projection_state(
            configs[config_index].xcentre,
            configs[config_index].ycentre,
            configs[config_index].near_clip,
            configs[config_index].ratio_x,
            configs[config_index].ratio_y,
            configs[config_index].camera_xr,
            configs[config_index].camera_yr,
            configs[config_index].camera_zr);

        for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
            char lbl[128];
            snprintf(lbl, sizeof(lbl), "LongProjectPoint3DF fixed cfg=%d x=%d y=%d z=%d", config_index, cases[i].x, cases[i].y, cases[i].z);
            assert_projection_case(lbl, cases[i].x, cases[i].y, cases[i].z);
        }
    }
}

static void test_random_equivalence(void)
{
    rng_seed(0xDEADBEEFu);

    for (int i = 0; i < 200; i++) {
        S32 near_clip = ((S32)(rng_next() % 64u)) + 1;
        S32 camera_zr = ((S32)(rng_next() % 2048u)) + near_clip + 32;

        configure_projection_state(
            (S32)(rng_next() % 641u),
            (S32)(rng_next() % 481u),
            near_clip,
            ((S32)(rng_next() % 512u)) + 64,
            ((S32)(rng_next() % 512u)) + 64,
            (S32)(rng_next() % 1025u) - 512,
            (S32)(rng_next() % 1025u) - 512,
            camera_zr);

        S32 x = (S32)(rng_next() % 8193u) - 4096;
        S32 y = (S32)(rng_next() % 8193u) - 4096;
        S32 z = (S32)(rng_next() % (U32)(camera_zr + near_clip + 1025));

        char lbl[128];
        snprintf(lbl, sizeof(lbl), "LongProjectPoint3DF rand x=%d y=%d z=%d", x, y, z);
        assert_projection_case(lbl, x, y, z);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
