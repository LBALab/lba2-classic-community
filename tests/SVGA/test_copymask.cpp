/* Test: CopyMask — copy source region through a mask shape.
 * Uses same bank format. Copies from source where mask has opaque pixels. */
#include "test_harness.h"
#include <SVGA/COPYMASK.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>
#include <stdio.h>

extern "C" void asm_CopyMask(S32 nummask, S32 x, S32 y, U8 *bankmask, void *src);

static U8 framebuf[640 * 480];
static U8 srcbuf[640 * 480];
static U8 g_bank[128];

static void setup_screen(void) {
    memset(framebuf, 0, sizeof(framebuf));
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++)
        TabOffLine[i] = i * 640;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = 639;
    ClipYMax = 479;
}

static void build_mask_bank(void) {
    memset(g_bank, 0, sizeof(g_bank));
    U32 *offsets = (U32 *)g_bank;
    offsets[0] = 4;
    U8 *b = g_bank + 4;
    b[0] = 4;
    b[1] = 1;
    b[2] = 0;
    b[3] = 0;
    /* Line 0: NbBlock=2 (1 skip/fill pair) */
    b[4] = 2;
    b[5] = 0; /* NbTransp */
    b[6] = 4; /* NbFill */
}

static void test_cpp_basic(void) {
    build_mask_bank();
    setup_screen();
    /* Fill source with gradient */
    for (int i = 0; i < 640 * 480; i++)
        srcbuf[i] = (U8)(i & 0xFF);
    CopyMask(0, 100, 100, g_bank, srcbuf);
    /* Pixels at (100,100)..(103,100) should be copied from source */
    ASSERT_EQ_UINT(srcbuf[100 * 640 + 100], framebuf[100 * 640 + 100]);
    ASSERT_EQ_UINT(srcbuf[100 * 640 + 103], framebuf[100 * 640 + 103]);
}

static void test_asm_equiv(void) {
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];
    build_mask_bank();
    for (int i = 0; i < 640 * 480; i++)
        srcbuf[i] = (U8)(i * 7 & 0xFF);

    setup_screen();
    CopyMask(0, 50, 50, g_bank, srcbuf);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    asm_CopyMask(0, 50, 50, g_bank, srcbuf);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "CopyMask");
}

static void test_random_positions(void) {
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];
    U32 rng = 0xABCD5678;
    int prev = test_failures;
    build_mask_bank();
    for (int i = 0; i < 100 && test_failures == prev; i++) {
        rng = rng * 1103515245u + 12345u;
        S32 mx = (S32)((rng >> 16) % 600);
        rng = rng * 1103515245u + 12345u;
        S32 my = (S32)((rng >> 16) % 460);

        for (int j = 0; j < 640 * 480; j++)
            srcbuf[j] = (U8)((j * 13 + i) & 0xFF);

        setup_screen();
        CopyMask(0, mx, my, g_bank, srcbuf);
        memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

        setup_screen();
        asm_CopyMask(0, mx, my, g_bank, srcbuf);
        memcpy(asm_buf, framebuf, sizeof(asm_buf));

        char label[64];
        snprintf(label, sizeof(label), "CopyMask random #%d (%d,%d)", i, mx, my);
        ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), label);
    }
}

/* Masks that cross the clip window, the top edge above all (#356: a brick
 * over a door at the top of the screen starts at a negative y). */
static U8 g_bank_tall[64];

static void build_tall_mask_bank(void) {
    memset(g_bank_tall, 0, sizeof(g_bank_tall));
    U32 *offsets = (U32 *)g_bank_tall;
    offsets[0] = 4;
    U8 *b = g_bank_tall + 4;
    b[0] = 12; /* DeltaX */
    b[1] = 4;  /* DeltaY */
    b[2] = 0;
    b[3] = 0;
    /* Each line: skip 2, copy 8, skip 2 (3 blocks), a different shape per line
     * would not matter here; the rows are told apart by the source. */
    for (int line = 0; line < 4; ++line) {
        b[4 + line * 4 + 0] = 3;
        b[4 + line * 4 + 1] = (U8)(line & 1);
        b[4 + line * 4 + 2] = 8;
        b[4 + line * 4 + 3] = (U8)(4 - (line & 1));
    }
}

static void test_clipped_positions(void) {
    static U8 cpp_buf[640 * 480];
    static U8 asm_buf[640 * 480];
    static const S32 pos[][2] = {
        {100, -1}, {100, -3}, {-5, -2}, {634, -2}, {100, 478}, {-5, 100}, {634, 100}, {100, 18}, {25, 40}, {100, -10}};
    static const S32 clip[][4] = {{0, 0, 639, 479}, {30, 20, 600, 470}};
    build_tall_mask_bank();
    for (int j = 0; j < 640 * 480; j++)
        srcbuf[j] = (U8)((j * 31 + 7) & 0xFF);
    for (unsigned c = 0; c < sizeof(clip) / sizeof(clip[0]); c++) {
        for (unsigned i = 0; i < sizeof(pos) / sizeof(pos[0]); i++) {
            setup_screen();
            ClipXMin = clip[c][0];
            ClipYMin = clip[c][1];
            ClipXMax = clip[c][2];
            ClipYMax = clip[c][3];
            CopyMask(0, pos[i][0], pos[i][1], g_bank_tall, srcbuf);
            memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

            setup_screen();
            ClipXMin = clip[c][0];
            ClipYMin = clip[c][1];
            ClipXMax = clip[c][2];
            ClipYMax = clip[c][3];
            asm_CopyMask(0, pos[i][0], pos[i][1], g_bank_tall, srcbuf);
            memcpy(asm_buf, framebuf, sizeof(asm_buf));

            char label[80];
            snprintf(label, sizeof(label), "CopyMask clipped (%d,%d) clip %u", pos[i][0], pos[i][1], c);
            ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), label);
        }
    }
}

int main(void) {
    RUN_TEST(test_cpp_basic);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_random_positions);
    RUN_TEST(test_clipped_positions);
    TEST_SUMMARY();
    return test_failures != 0;
}
