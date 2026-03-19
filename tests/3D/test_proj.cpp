/* Test: SetProjection / SetIsoProjection - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <3D/LPROJ.H>
#include <string.h>

extern "C" void asm_SetProjection(S32 xc, S32 yc, S32 clip, S32 fx, S32 fy);
extern "C" void asm_SetIsoProjection(S32 xc, S32 yc);

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static U32 float_bits(float value) {
    U32 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void seed_projection_globals(void) {
    XCentre = -111;
    YCentre = -222;
    NearClip = -333;
    LFactorX = -444;
    LFactorY = -555;
    TypeProj = -666;
    FRatioX = 123.25f;
    FRatioY = -456.5f;
}

static void assert_set_projection_case(const char *label, S32 xc, S32 yc, S32 clip, S32 fx, S32 fy) {
    seed_projection_globals();
    asm_SetProjection(xc, yc, clip, fx, fy);
    S32 asm_xc = XCentre;
    S32 asm_yc = YCentre;
    S32 asm_clip = NearClip;
    S32 asm_fx = LFactorX;
    S32 asm_fy = LFactorY;
    S32 asm_type = TypeProj;
    U32 asm_frx = float_bits(FRatioX);
    U32 asm_fry = float_bits(FRatioY);

    seed_projection_globals();
    SetProjection(xc, yc, clip, fx, fy);

    ASSERT_ASM_CPP_EQ_INT(asm_xc, XCentre, label);
    ASSERT_ASM_CPP_EQ_INT(asm_yc, YCentre, label);
    ASSERT_ASM_CPP_EQ_INT(asm_clip, NearClip, label);
    ASSERT_ASM_CPP_EQ_INT(asm_fx, LFactorX, label);
    ASSERT_ASM_CPP_EQ_INT(asm_fy, LFactorY, label);
    ASSERT_ASM_CPP_EQ_INT(asm_type, TypeProj, label);
    ASSERT_EQ_UINT(asm_frx, float_bits(FRatioX));
    ASSERT_EQ_UINT(asm_fry, float_bits(FRatioY));
}

static void assert_set_iso_projection_case(const char *label, S32 xc, S32 yc) {
    seed_projection_globals();
    asm_SetIsoProjection(xc, yc);
    S32 asm_xc = XCentre;
    S32 asm_yc = YCentre;
    S32 asm_clip = NearClip;
    S32 asm_fx = LFactorX;
    S32 asm_fy = LFactorY;
    S32 asm_type = TypeProj;
    U32 asm_frx = float_bits(FRatioX);
    U32 asm_fry = float_bits(FRatioY);

    seed_projection_globals();
    SetIsoProjection(xc, yc);

    ASSERT_ASM_CPP_EQ_INT(asm_xc, XCentre, label);
    ASSERT_ASM_CPP_EQ_INT(asm_yc, YCentre, label);
    ASSERT_ASM_CPP_EQ_INT(asm_clip, NearClip, label);
    ASSERT_ASM_CPP_EQ_INT(asm_fx, LFactorX, label);
    ASSERT_ASM_CPP_EQ_INT(asm_fy, LFactorY, label);
    ASSERT_ASM_CPP_EQ_INT(asm_type, TypeProj, label);
    ASSERT_EQ_UINT(asm_frx, float_bits(FRatioX));
    ASSERT_EQ_UINT(asm_fry, float_bits(FRatioY));
}

static void test_setprojection_equivalence(void) {
    struct {
        S32 xc, yc, cl, fx, fy;
    } cases[] = {
        {320, 240, 10, 500, 400},
        {0, 0, 1, 100, 100},
        {640, 480, 50, 1000, 800},
        {123, 456, 7, 321, 654},
        {1, 2, 255, 4096, 2048},
    };
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "SetProjection fixed xc=%d yc=%d cl=%d fx=%d fy=%d",
                 cases[i].xc, cases[i].yc, cases[i].cl, cases[i].fx, cases[i].fy);
        assert_set_projection_case(lbl, cases[i].xc, cases[i].yc, cases[i].cl, cases[i].fx, cases[i].fy);
    }
}

static void test_setprojection_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; i++) {
        S32 xc = (S32)(rng_next() % 1280u);
        S32 yc = (S32)(rng_next() % 960u);
        S32 cl = (S32)(rng_next() % 255u) + 1;
        S32 fx = (S32)(rng_next() % 4096u) + 1;
        S32 fy = (S32)(rng_next() % 4096u) + 1;
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "SetProjection rand xc=%d yc=%d cl=%d fx=%d fy=%d", xc, yc, cl, fx, fy);
        assert_set_projection_case(lbl, xc, yc, cl, fx, fy);
    }
}

static void test_setisoprojection_equivalence(void) {
    struct {
        S32 xc, yc;
    } cases[] = {
        {320, 240},
        {0, 0},
        {640, 480},
        {123, 456},
        {-50, 75},
    };

    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "SetIsoProjection fixed xc=%d yc=%d", cases[i].xc, cases[i].yc);
        assert_set_iso_projection_case(lbl, cases[i].xc, cases[i].yc);
    }
}

static void test_setisoprojection_random_equivalence(void) {
    rng_seed(0xC0FFEEu);
    for (int i = 0; i < 200; ++i) {
        S32 xc = (S32)(rng_next() % 1280u) - 320;
        S32 yc = (S32)(rng_next() % 960u) - 240;
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "SetIsoProjection rand xc=%d yc=%d", xc, yc);
        assert_set_iso_projection_case(lbl, xc, yc);
    }
}

int main(void) {
    RUN_TEST(test_setprojection_equivalence);
    RUN_TEST(test_setprojection_random_equivalence);
    RUN_TEST(test_setisoprojection_equivalence);
    RUN_TEST(test_setisoprojection_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
