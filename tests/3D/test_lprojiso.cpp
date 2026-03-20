/* Test: LongProjectPointIso - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/CAMERA.H>

extern "C" void asm_LongProjectPointIso(void);
static S32 call_asm_LongProjectPointIso(S32 x, S32 y, S32 z) {
    S32 result;
    __asm__ __volatile__("call asm_LongProjectPointIso" : "=a"(result) : "0"(x), "b"(y), "c"(z) : "memory", "edx");
    return result;
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void configure_iso_state(S32 camera_x, S32 camera_y, S32 camera_z, S32 xcentre, S32 ycentre) {
    CameraX = camera_x;
    CameraY = camera_y;
    CameraZ = camera_z;
    XCentre = xcentre;
    YCentre = ycentre;
}

static void assert_iso_case(const char *label, S32 x, S32 y, S32 z) {
    S32 cpp_ret = LongProjectPointIso(x, y, z);
    S32 cpp_xp = Xp;
    S32 cpp_yp = Yp;
    S32 asm_ret = call_asm_LongProjectPointIso(x, y, z);

    ASSERT_ASM_CPP_EQ_INT(asm_ret, cpp_ret, label);
    ASSERT_ASM_CPP_EQ_INT(Xp, cpp_xp, label);
    ASSERT_ASM_CPP_EQ_INT(Yp, cpp_yp, label);
}

static void test_equivalence(void) {
    struct {
        S32 camera_x, camera_y, camera_z, xcentre, ycentre;
    } configs[] = {
        {0, 0, 0, 320, 240},
        {100, -50, 25, 160, 100},
        {-256, 512, -128, 400, 300},
    };
    struct {
        S32 x, y, z;
    } cases[] = {
        {0, 0, 0},
        {64, 0, 0},
        {0, 64, 0},
        {0, 0, 64},
        {100, 50, 200},
        {-100, -50, -200},
        {1024, -512, 2048},
        {-4096, 2048, -1024},
    };

    for (int config_index = 0; config_index < (int)(sizeof(configs) / sizeof(configs[0])); ++config_index) {
        configure_iso_state(
            configs[config_index].camera_x,
            configs[config_index].camera_y,
            configs[config_index].camera_z,
            configs[config_index].xcentre,
            configs[config_index].ycentre);

        for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
            char lbl[128];
            snprintf(lbl, sizeof(lbl), "LongProjectPointIso fixed cfg=%d x=%d y=%d z=%d", config_index, cases[i].x, cases[i].y, cases[i].z);
            assert_iso_case(lbl, cases[i].x, cases[i].y, cases[i].z);
        }
    }
}

static void test_shift_boundary_equivalence(void) {
    struct {
        S32 x, y, z;
    } cases[] = {
        {0, 0, 0},
        {1, 0, 0},
        {-1, 0, 0},
        {63, 0, 0},
        {64, 0, 0},
        {65, 0, 0},
        {-63, 0, 0},
        {-64, 0, 0},
        {-65, 0, 0},
        {0, 17, 0},
        {0, -17, 0},
        {31, 15, -7},
        {-31, -15, 7},
    };

    configure_iso_state(0, 0, 0, 320, 240);
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        char lbl[112];
        snprintf(lbl, sizeof(lbl), "LongProjectPointIso shift boundary x=%d y=%d z=%d", cases[i].x, cases[i].y, cases[i].z);
        assert_iso_case(lbl, cases[i].x, cases[i].y, cases[i].z);
    }
}

static void test_camera_cancel_equivalence(void) {
    configure_iso_state(123, -77, 19, 160, 100);
    assert_iso_case("LongProjectPointIso camera cancellation", CameraX, CameraY, CameraZ);
    assert_iso_case("LongProjectPointIso camera cancellation offset", CameraX + 64, CameraY - 32, CameraZ + 16);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);

    for (int i = 0; i < 200; i++) {
        configure_iso_state(
            (S32)(rng_next() % 2049u) - 1024,
            (S32)(rng_next() % 2049u) - 1024,
            (S32)(rng_next() % 2049u) - 1024,
            (S32)(rng_next() % 641u),
            (S32)(rng_next() % 481u));

        S32 x = (S32)(rng_next() % 8193u) - 4096;
        S32 y = (S32)(rng_next() % 8193u) - 4096;
        S32 z = (S32)(rng_next() % 8193u) - 4096;

        char lbl[128];
        snprintf(lbl, sizeof(lbl), "LongProjectPointIso rand x=%d y=%d z=%d", x, y, z);
        assert_iso_case(lbl, x, y, z);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_shift_boundary_equivalence);
    RUN_TEST(test_camera_cancel_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
