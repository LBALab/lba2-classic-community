/* Test: LongProjectPoint3DI - ASM vs CPP equivalence (the 1997 integer path) */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/CAMERA.H>

extern "C" void asm_LongProjectPoint3DI(void);
static S32 call_asm_LongProjectPoint3DI(S32 x, S32 y, S32 z) {
    /* Watcom: parm [eax] [ebx] [ecx], modify [edx]; the error path also writes ebx. */
    S32 result;
    __asm__ __volatile__("call asm_LongProjectPoint3DI"
                         : "=a"(result), "+b"(y), "+c"(z) : "0"(x) : "memory", "edx");
    return result;
}

static U32 rng_state;

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state;
}

static void set_camera(S32 xr, S32 yr, S32 zr, S32 nearClip, S32 fx, S32 fy, S32 xc, S32 yc) {
    CameraXr = xr;
    CameraYr = yr;
    CameraZr = zr;
    NearClip = nearClip;
    CameraZrClip = zr - nearClip;
    LFactorX = fx;
    LFactorY = fy;
    XCentre = xc;
    YCentre = yc;
}

static void assert_projection_case(const char *label, S32 x, S32 y, S32 z) {
    Xp = Yp = 0x13579BDF;
    S32 cpp_ret = LongProjectPoint3DI(x, y, z);
    S32 cpp_xp = Xp, cpp_yp = Yp;

    Xp = Yp = 0x2468ACE0;
    S32 asm_ret = call_asm_LongProjectPoint3DI(x, y, z);

    ASSERT_ASM_CPP_EQ_INT(asm_ret, cpp_ret, label);
    ASSERT_ASM_CPP_EQ_INT(Xp, cpp_xp, label);
    ASSERT_ASM_CPP_EQ_INT(Yp, cpp_yp, label);
}

/* Both branches, the clip boundary on either side, and quotients of both signs
   (idiv truncates toward zero, so negative ones round up). */
static void test_fixed(void) {
    set_camera(1200, -800, 20000, 1, 600, 600, 368, 240);
    const S32 cases[][3] = {
        {1200, -800, 19999}, /* z == CameraZrClip: projects */
        {1200, -800, 20000}, /* z == CameraZr: clipped */
        {1200, -800, 20001},
        {0, 0, 0},
        {1201, -799, 19000},
        {1199, -801, 19000},
        {5000, 3000, -40000},
        {-5000, -3000, 19990},
        {1200 + 7, -800 - 7, 20000 - 3}, /* 7 * 600 / -3 exactly */
        {1200 + 1, -800 + 1, 20000 - 7}, /* 600 / -7, a fraction each way */
    };
    char lbl[96];
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        snprintf(lbl, sizeof(lbl), "LongProjectPoint3DI fixed x=%d y=%d z=%d", cases[i][0], cases[i][1], cases[i][2]);
        assert_projection_case(lbl, cases[i][0], cases[i][1], cases[i][2]);
    }
}

/* Cameras and points over the exterior's range, both sides of the clip. */
static void test_random(void) {
    rng_state = 0x0DDBA11u;
    char lbl[128];
    for (int i = 0; i < 100000; i++) {
        S32 nearClip = 1 + (S32)(rng_next() % 64u);
        S32 zr = (S32)(rng_next() % 200000u) - 100000;
        set_camera((S32)(rng_next() % 200000u) - 100000, (S32)(rng_next() % 40000u) - 20000, zr, nearClip,
                   200 + (S32)(rng_next() % 1600u), 200 + (S32)(rng_next() % 1600u), (S32)(rng_next() % 1920u),
                   (S32)(rng_next() % 1080u));
        S32 x = CameraXr + (S32)(rng_next() % 200000u) - 100000;
        S32 y = CameraYr + (S32)(rng_next() % 40000u) - 20000;
        S32 z = zr - (S32)(rng_next() % 120000u) + 1000;
        snprintf(lbl, sizeof(lbl), "LongProjectPoint3DI rand i=%d x=%d y=%d z=%d", i, x, y, z);
        assert_projection_case(lbl, x, y, z);
    }
}

int main(void) {
    RUN_TEST(test_fixed);
    RUN_TEST(test_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
