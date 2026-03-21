/* Test: ProjectListIso - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>
#include <string.h>

extern void ProjectListIso(TYPE_PT *d, TYPE_VT16 *s, S32 n, S32 ox, S32 oy, S32 oz);
extern "C" void asm_ProjectListIso(void);
static void call_asm_ProjectListIso(TYPE_PT *d, TYPE_VT16 *s, S32 n, S32 ox, S32 oy, S32 oz) {
    S32 _a, _b, _d;
    __asm__ __volatile__(
        "pushl %%edx\n\t"
        "pushl %%ebx\n\t"
        "pushl %%eax\n\t"
        "call asm_ProjectListIso\n\t"
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

static void assert_project_iso_case(const char *label, TYPE_VT16 *src, S32 n, S32 ox, S32 oy, S32 oz) {
    TYPE_PT cpp_dst[8];
    TYPE_PT asm_dst[8];
    TYPE_VT16 cpp_src[8];
    TYPE_VT16 asm_src[8];

    ASSERT_TRUE(n >= 0 && n <= 8);

    memset(cpp_dst, 0xCC, sizeof(cpp_dst));
    memset(asm_dst, 0xCC, sizeof(asm_dst));
    memcpy(cpp_src, src, (size_t)n * sizeof(TYPE_VT16));
    memcpy(asm_src, src, (size_t)n * sizeof(TYPE_VT16));

    reset_screen_bounds();
    ProjectListIso(cpp_dst, cpp_src, n, ox, oy, oz);
    S32 cpp_xmin = ScreenXMin;
    S32 cpp_xmax = ScreenXMax;
    S32 cpp_ymin = ScreenYMin;
    S32 cpp_ymax = ScreenYMax;

    reset_screen_bounds();
    call_asm_ProjectListIso(asm_dst, asm_src, n, ox, oy, oz);

    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, (size_t)n * sizeof(TYPE_PT), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_src, cpp_src, (size_t)n * sizeof(TYPE_VT16), label);
    ASSERT_ASM_CPP_EQ_INT(ScreenXMin, cpp_xmin, label);
    ASSERT_ASM_CPP_EQ_INT(ScreenXMax, cpp_xmax, label);
    ASSERT_ASM_CPP_EQ_INT(ScreenYMin, cpp_ymin, label);
    ASSERT_ASM_CPP_EQ_INT(ScreenYMax, cpp_ymax, label);
}

static void test_equivalence(void) {
    struct {
        S32 xcentre, ycentre, ox, oy, oz;
    } configs[] = {
        {320, 240, 0, 0, 0},
        {160, 100, 10, -20, 30},
        {400, 300, -100, 50, -75},
    };
    TYPE_VT16 cases[][5] = {
        {{0, 0, 0, 0}, {100, 50, 100, 0}, {-100, -50, -100, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{300, -120, 40, 0}, {-300, 120, -80, 0}, {0, 0, 64, 0}, {100, 50, -300, 0}, {-100, -50, 30, 0}},
        {{2000, 0, -100, 0}, {-2000, 0, 100, 0}, {0, 2000, -100, 0}, {0, -2000, 100, 0}, {25, 25, 4000, 0}},
    };
    S32 counts[] = {3, 5, 5};

    for (int config_index = 0; config_index < (int)(sizeof(configs) / sizeof(configs[0])); ++config_index) {
        XCentre = configs[config_index].xcentre;
        YCentre = configs[config_index].ycentre;

        char lbl[96];
        snprintf(lbl, sizeof(lbl), "ProjectListIso fixed cfg=%d", config_index);
        assert_project_iso_case(lbl, cases[config_index], counts[config_index],
                                configs[config_index].ox, configs[config_index].oy,
                                configs[config_index].oz);
    }
}

static void test_sortkey_and_overflow_equivalence(void) {
    TYPE_VT16 sortkey_cases[] = {
        {100, 20, 101, 0x1234},
        {-100, 20, -101, 0x4321},
        {32767, 0, 32767, 0x7FFF},
        {-32768, 0, -32768, 0x5555},
        {50, -50, -49, 0x2222},
    };
    TYPE_VT16 overflow_cases[] = {
        {2048, 0, -2048, 0x1111},
        {-2048, 0, 2048, 0x2222},
        {0, 4096, 0, 0x3333},
        {16, 0, 16, 0x4444},
    };

    XCentre = 320;
    YCentre = 240;
    assert_project_iso_case("ProjectListIso sort-key overwrite", sortkey_cases, 5, 0, 0, 0);

    XCentre = 32760;
    YCentre = 32760;
    assert_project_iso_case("ProjectListIso mixed overflow", overflow_cases, 4, 0, 0, 0);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);

    for (int i = 0; i < 200; i++) {
        TYPE_VT16 src[8];
        S32 n = (S32)(rng_next() % 8u) + 1;

        XCentre = (S32)(rng_next() % 641u);
        YCentre = (S32)(rng_next() % 481u);

        S32 ox = (S32)(rng_next() % 1025u) - 512;
        S32 oy = (S32)(rng_next() % 1025u) - 512;
        S32 oz = (S32)(rng_next() % 1025u) - 512;

        for (S32 index = 0; index < n; ++index) {
            src[index].X = (S16)((S32)(rng_next() % 8193u) - 4096);
            src[index].Y = (S16)((S32)(rng_next() % 8193u) - 4096);
            src[index].Z = (S16)((S32)(rng_next() % 8193u) - 4096);
            src[index].Grp = 0;
        }

        char lbl[128];
        snprintf(lbl, sizeof(lbl), "ProjectListIso rand n=%d oz=%d", n, oz);
        assert_project_iso_case(lbl, src, n, ox, oy, oz);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_sortkey_and_overflow_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
