#include "test_harness.h"

#include "FLOW.H"

#include <POLYGON/POLY.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

#include <string.h>

extern "C" void asm_BoxFlow(void);
extern "C" void asm_ShadeBoxBlk(S32 x0, S32 y0, S32 x1, S32 y1, S32 deccoul);
extern "C" void asm_CopyBlockShade(void);
extern "C" void call_asm_BoxFlow(S32 x, S32 y, S32 coul);
extern "C" void call_asm_ShadeBoxBlk(S32 x0, S32 y0, S32 x1, S32 y1, S32 deccoul);
extern "C" void call_asm_CopyBlockShade(S32 x0, S32 y0, S32 x1, S32 y1,
                                        U8 *src, S32 xd, S32 yd, U8 *dst, S32 shade);

__asm__(
    ".globl call_asm_BoxFlow\n"
    "call_asm_BoxFlow:\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    pushl %ebx\n"
    "    movl 8(%ebp), %eax\n"
    "    movl 12(%ebp), %ebx\n"
    "    movl 16(%ebp), %ecx\n"
    "    call asm_BoxFlow\n"
    "    popl %ebx\n"
    "    popl %ebp\n"
    "    ret\n");

__asm__(
    ".globl call_asm_ShadeBoxBlk\n"
    "call_asm_ShadeBoxBlk:\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    pushl 24(%ebp)\n"
    "    pushl 20(%ebp)\n"
    "    pushl 16(%ebp)\n"
    "    pushl 12(%ebp)\n"
    "    pushl 8(%ebp)\n"
    "    call asm_ShadeBoxBlk\n"
    "    addl $20, %esp\n"
    "    popl %ebp\n"
    "    ret\n");

__asm__(
    ".globl call_asm_CopyBlockShade\n"
    "call_asm_CopyBlockShade:\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    pushl %ebx\n"
    "    pushl %edi\n"
    "    pushl %esi\n"
    "    movl 8(%ebp), %edx\n"
    "    movl 12(%ebp), %ecx\n"
    "    movl 16(%ebp), %ebx\n"
    "    movl 20(%ebp), %eax\n"
    "    movl 24(%ebp), %esi\n"
    "    pushl 40(%ebp)\n"
    "    pushl 36(%ebp)\n"
    "    pushl 32(%ebp)\n"
    "    pushl 28(%ebp)\n"
    "    call asm_CopyBlockShade\n"
    "    addl $16, %esp\n"
    "    popl %esi\n"
    "    popl %edi\n"
    "    popl %ebx\n"
    "    popl %ebp\n"
    "    ret\n");

void *Log = 0;
U32 TabOffLine[ADELINE_MAX_Y_RES];
S32 ClipXMin = 0;
S32 ClipYMin = 0;
S32 ClipXMax = 639;
S32 ClipYMax = 479;
U8 *PtrCLUTGouraud = 0;
void *Screen = 0;
U32 ModeDesiredX = 640;
U32 ModeDesiredY = 480;

static U8 clut_gouraud[16 * 256];
static U32 rng_state;

static void rng_seed(U32 seed_value) {
    rng_state = seed_value;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void init_globals(void) {
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; ++i) {
        TabOffLine[i] = i * 640;
    }
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = 639;
    ClipYMax = 479;
    PtrCLUTGouraud = clut_gouraud;

    for (U32 row = 0; row < 16; ++row) {
        for (U32 value = 0; value < 256; ++value) {
            clut_gouraud[row * 256 + value] = (U8)((value + row) & 0xFFu);
        }
    }
}

static void fill_random(U8 *buffer, U32 size) {
    for (U32 i = 0; i < size; ++i) {
        buffer[i] = (U8)rng_next();
    }
}

static void test_boxflow_equivalence(void) {
    U8 cpp_log[640 * 480];
    U8 asm_log[640 * 480];
    struct Case {
        S32 x;
        S32 y;
        S32 color;
    } cases[] = {
        {10, 10, 0x44},
        {0, 0, 0x7F},
        {638, 478, 0x33},
        {-1, 10, 0x55},
        {10, 479, 0x66}};

    init_globals();
    for (U32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        memset(cpp_log, 0x11, sizeof(cpp_log));
        memset(asm_log, 0x11, sizeof(asm_log));

        Log = cpp_log;
        BoxFlow(cases[i].x, cases[i].y, cases[i].color);

        Log = asm_log;
        call_asm_BoxFlow(cases[i].x, cases[i].y, cases[i].color);

        ASSERT_ASM_CPP_MEM_EQ(asm_log, cpp_log, sizeof(cpp_log), "BoxFlow");
    }
}

static void test_boxflow_random_stress(void) {
    U8 cpp_log[640 * 480];
    U8 asm_log[640 * 480];

    init_globals();
    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 300; ++round) {
        S32 x = (S32)(rng_next() % 700) - 30;
        S32 y = (S32)(rng_next() % 540) - 30;
        S32 color = (S32)(rng_next() & 0xFFu);

        memset(cpp_log, 0x22, sizeof(cpp_log));
        memset(asm_log, 0x22, sizeof(asm_log));

        Log = cpp_log;
        BoxFlow(x, y, color);

        Log = asm_log;
        call_asm_BoxFlow(x, y, color);

        ASSERT_ASM_CPP_MEM_EQ(asm_log, cpp_log, sizeof(cpp_log), "BoxFlow random");
    }
}

static void test_shadeboxblk_equivalence(void) {
    U8 cpp_log[640 * 480 + 640];
    U8 asm_log[640 * 480 + 640];
    struct Case {
        S32 x0;
        S32 y0;
        S32 x1;
        S32 y1;
        S32 dec;
    } cases[] = {
        {10, 10, 20, 20, 1},
        {-5, 5, 8, 12, 3},
        {630, 470, 638, 478, 7},
        {0, 0, 638, 478, 5}};

    init_globals();
    rng_seed(0x12345678u);
    for (U32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        fill_random(cpp_log, sizeof(cpp_log));
        memcpy(asm_log, cpp_log, sizeof(cpp_log));

        Log = cpp_log;
        ShadeBoxBlk(cases[i].x0, cases[i].y0, cases[i].x1, cases[i].y1, cases[i].dec);

        Log = asm_log;
        call_asm_ShadeBoxBlk(cases[i].x0, cases[i].y0, cases[i].x1, cases[i].y1, cases[i].dec);

        ASSERT_ASM_CPP_MEM_EQ(asm_log, cpp_log, 640 * 480, "ShadeBoxBlk");
    }
}

static void test_shadeboxblk_random_stress(void) {
    U8 cpp_log[640 * 480 + 640];
    U8 asm_log[640 * 480 + 640];

    init_globals();
    rng_seed(0xBADC0DEu);
    for (int round = 0; round < 300; ++round) {
        S32 x0 = (S32)(rng_next() % 620);
        S32 y0 = (S32)(rng_next() % 460);
        S32 x1 = x0 + (S32)(rng_next() % 19);
        S32 y1 = y0 + (S32)(rng_next() % 19);
        S32 dec = (S32)(rng_next() & 0x0Fu);

        fill_random(cpp_log, sizeof(cpp_log));
        memcpy(asm_log, cpp_log, sizeof(cpp_log));

        Log = cpp_log;
        ShadeBoxBlk(x0, y0, x1, y1, dec);

        Log = asm_log;
        call_asm_ShadeBoxBlk(x0, y0, x1, y1, dec);

        ASSERT_ASM_CPP_MEM_EQ(asm_log, cpp_log, 640 * 480, "ShadeBoxBlk random");
    }
}

static void test_copyblockshade_equivalence(void) {
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];
    U8 src[640 * 480];
    struct Case {
        S32 x0;
        S32 y0;
        S32 x1;
        S32 y1;
        S32 xd;
        S32 yd;
        S32 shade;
    } cases[] = {
        {10, 10, 20, 20, 30, 30, 2},
        {0, 0, 0, 0, 100, 100, 1},
        {100, 50, 120, 75, 200, 150, 7},
        {10, 10, 700, 10, 0, 0, 3},
        {20, 20, 25, 19, 40, 40, 4},
        {10, 10, 20, 500, 50, 50, 5}};

    init_globals();
    rng_seed(0xCAFEBABEu);
    fill_random(src, sizeof(src));
    for (U32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        fill_random(cpp_dst, sizeof(cpp_dst));
        memcpy(asm_dst, cpp_dst, sizeof(cpp_dst));

        CopyBlockShade(cases[i].x0, cases[i].y0, cases[i].x1, cases[i].y1,
                       src, cases[i].xd, cases[i].yd, cpp_dst, cases[i].shade);
        call_asm_CopyBlockShade(cases[i].x0, cases[i].y0, cases[i].x1, cases[i].y1,
                                src, cases[i].xd, cases[i].yd, asm_dst, cases[i].shade);

        ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "CopyBlockShade dst");
    }
}

static void test_copyblockshade_random_stress(void) {
    U8 cpp_dst[640 * 480];
    U8 asm_dst[640 * 480];
    U8 src[640 * 480];

    init_globals();
    rng_seed(0x0D15EA5Eu);
    fill_random(src, sizeof(src));
    for (int round = 0; round < 300; ++round) {
        S32 x0 = (S32)(rng_next() % 620);
        S32 y0 = (S32)(rng_next() % 460);
        S32 x1 = x0 + (S32)(rng_next() % 32);
        S32 y1 = y0 + (S32)(rng_next() % 16);
        S32 xd = (S32)(rng_next() % (640 - (x1 - x0 + 1)));
        S32 yd = (S32)(rng_next() % (480 - (y1 - y0 + 1)));
        S32 shade = (S32)(rng_next() & 0x0Fu);

        fill_random(cpp_dst, sizeof(cpp_dst));
        memcpy(asm_dst, cpp_dst, sizeof(cpp_dst));

        CopyBlockShade(x0, y0, x1, y1, src, xd, yd, cpp_dst, shade);
        call_asm_CopyBlockShade(x0, y0, x1, y1, src, xd, yd, asm_dst, shade);

        ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "CopyBlockShade random");
    }
}

int main(void) {
    RUN_TEST(test_boxflow_equivalence);
    RUN_TEST(test_boxflow_random_stress);
    RUN_TEST(test_shadeboxblk_equivalence);
    RUN_TEST(test_shadeboxblk_random_stress);
    RUN_TEST(test_copyblockshade_equivalence);
    RUN_TEST(test_copyblockshade_random_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}