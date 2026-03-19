/* Test: ProjectList3DF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>
#include <string.h>

extern void ProjectList3DF(TYPE_PT *d, TYPE_VT16 *s, S32 n, S32 ox, S32 oy, S32 oz);
extern "C" void asm_ProjectList3DF(void);
static void call_asm_ProjectList3DF(TYPE_PT *d, TYPE_VT16 *s, S32 n, S32 ox, S32 oy, S32 oz) {
    S32 _a, _b, _d;
    __asm__ __volatile__(
        "pushl %%edx\n\t"
        "pushl %%ebx\n\t"
        "pushl %%eax\n\t"
        "call asm_ProjectList3DF\n\t"
        "addl $12, %%esp"
        : "=a"(_a), "=b"(_b), "=d"(_d)
        : "0"(ox), "1"(oy), "2"(oz), "D"(d), "S"(s), "c"(n) : "memory");
}

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void reset_screen_bounds(void) {
    ScreenXMin = 32767;
    ScreenXMax = -32767;
    ScreenYMin = 32767;
    ScreenYMax = -32767;
}

static void assert_project_list_case(const char *label, TYPE_VT16 *src, S32 n, S32 ox, S32 oy, S32 oz) {
    TYPE_PT cpp_dst[8];
    TYPE_PT asm_dst[8];

    ASSERT_TRUE(n >= 0 && n <= 8);

    memset(cpp_dst, 0xCC, sizeof(cpp_dst));
    memset(asm_dst, 0xCC, sizeof(asm_dst));

    reset_screen_bounds();
    ProjectList3DF(cpp_dst, src, n, ox, oy, oz);
    S32 cpp_xmin = ScreenXMin;
    S32 cpp_xmax = ScreenXMax;
    S32 cpp_ymin = ScreenYMin;
    S32 cpp_ymax = ScreenYMax;
    S32 cpp_xp = Xp;
    S32 cpp_yp = Yp;
    S32 cpp_x0 = X0;
    S32 cpp_y0 = Y0;
    S32 cpp_z0 = Z0;

    reset_screen_bounds();
    call_asm_ProjectList3DF(asm_dst, src, n, ox, oy, oz);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, (size_t)n * sizeof(TYPE_PT), label);
    ASSERT_ASM_CPP_EQ_INT(ScreenXMin, cpp_xmin, label);
    ASSERT_ASM_CPP_EQ_INT(ScreenXMax, cpp_xmax, label);
    ASSERT_ASM_CPP_EQ_INT(ScreenYMin, cpp_ymin, label);
    ASSERT_ASM_CPP_EQ_INT(ScreenYMax, cpp_ymax, label);
    ASSERT_ASM_CPP_EQ_INT(Xp, cpp_xp, label);
    ASSERT_ASM_CPP_EQ_INT(Yp, cpp_yp, label);
    ASSERT_ASM_CPP_EQ_INT(X0, cpp_x0, label);
    ASSERT_ASM_CPP_EQ_INT(Y0, cpp_y0, label);
    ASSERT_ASM_CPP_EQ_INT(Z0, cpp_z0, label);
}

static void test_equivalence(void) {
    struct {
        S32 xcentre, ycentre, near_clip, ratio_x, ratio_y;
        S32 ox, oy, oz;
    } configs[] = {
        {320, 240, 1, 300, 300, 0, 0, 0},
        {160, 100, 8, 256, 192, 10, -20, 200},
        {400, 300, 32, 512, 384, -100, 50, 1024},
    };
    TYPE_VT16 cases[][5] = {
        {{0, 0, -50, 0}, {10, 10, -100, 0}, {-10, -10, -200, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{300, -120, -1, 0}, {-300, 120, -600, 0}, {0, 0, -16, 0}, {100, 50, -300, 0}, {-100, -50, -30, 0}},
        {{2000, 0, -100, 0}, {-2000, 0, -100, 0}, {0, 2000, -100, 0}, {0, -2000, -100, 0}, {25, 25, -4000, 0}},
    };
    S32 counts[] = {3, 5, 5};

    for (int config_index = 0; config_index < (int)(sizeof(configs) / sizeof(configs[0])); ++config_index) {
        SetProjection(configs[config_index].xcentre, configs[config_index].ycentre,
                      configs[config_index].near_clip, configs[config_index].ratio_x,
                      configs[config_index].ratio_y);

        char lbl[96];
        snprintf(lbl, sizeof(lbl), "ProjectList3DF fixed cfg=%d", config_index);
        assert_project_list_case(lbl, cases[config_index], counts[config_index],
                                 configs[config_index].ox, configs[config_index].oy,
                                 configs[config_index].oz);
    }
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);

    for (int i = 0; i < 200; i++) {
        TYPE_VT16 src[8];
        S32 n = (S32)(rng_next() % 8u) + 1;
        S32 near_clip = ((S32)(rng_next() % 64u)) + 1;

        SetProjection((S32)(rng_next() % 641u), (S32)(rng_next() % 481u), near_clip,
                      ((S32)(rng_next() % 512u)) + 64, ((S32)(rng_next() % 512u)) + 64);

        S32 ox = (S32)(rng_next() % 1025u) - 512;
        S32 oy = (S32)(rng_next() % 1025u) - 512;
        S32 oz = (S32)(rng_next() % 2049u) - 512;

        for (S32 index = 0; index < n; ++index) {
            src[index].X = (S16)((S32)(rng_next() % 8193u) - 4096);
            src[index].Y = (S16)((S32)(rng_next() % 8193u) - 4096);
            src[index].Z = (S16)((S32)(rng_next() % 8193u) - 4096);
            src[index].Grp = 0;
        }

        char lbl[128];
        snprintf(lbl, sizeof(lbl), "ProjectList3DF rand n=%d oz=%d", n, oz);
        assert_project_list_case(lbl, src, n, ox, oy, oz);
    }
}

static void test_zero_count_preserves_bounds(void) {
    TYPE_VT16 src[1] = {{1, 2, 3, 0}};

    SetProjection(320, 240, 1, 300, 300);
    ScreenXMin = 10;
    ScreenXMax = 20;
    ScreenYMin = 30;
    ScreenYMax = 40;
    ProjectList3DF(NULL, src, 0, 0, 0, 0);
    S32 cpp_xmin = ScreenXMin;
    S32 cpp_xmax = ScreenXMax;
    S32 cpp_ymin = ScreenYMin;
    S32 cpp_ymax = ScreenYMax;

    ScreenXMin = 10;
    ScreenXMax = 20;
    ScreenYMin = 30;
    ScreenYMax = 40;
    call_asm_ProjectList3DF(NULL, src, 0, 0, 0, 0);

    ASSERT_ASM_CPP_EQ_INT(ScreenXMin, cpp_xmin, "ProjectList3DF zero-count XMin");
    ASSERT_ASM_CPP_EQ_INT(ScreenXMax, cpp_xmax, "ProjectList3DF zero-count XMax");
    ASSERT_ASM_CPP_EQ_INT(ScreenYMin, cpp_ymin, "ProjectList3DF zero-count YMin");
    ASSERT_ASM_CPP_EQ_INT(ScreenYMax, cpp_ymax, "ProjectList3DF zero-count YMax");
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_zero_count_preserves_bounds);
    TEST_SUMMARY();
    return test_failures != 0;
}
