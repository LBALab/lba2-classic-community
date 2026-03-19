/* Test: AffMask — draw mask/sprite with single color.
 * Uses the same brick bank format as AffGraph. Tests rendering to framebuffer.
 * Uses a simple unclipped brick with all-copy opcode. */
#include "test_harness.h"
#include <SVGA/MASK.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREENXY.H>
#include <string.h>
#include <stdio.h>

/* ASM AffMask uses Watcom register convention:
 *   parm [eax]=nummask [ebx]=x [ecx]=y [esi]=bankmask
 * We need a wrapper that takes C stack params and forwards to ASM regs. */
extern "C" void asm_AffMask(void);
extern "C" U8 asm_ColMask;

static S32 call_asm_AffMask(S32 num, S32 x, S32 y, void *bank) {
    S32 result;
    __asm__ __volatile__(
        "call asm_AffMask"
        : "=a"(result)
        : "a"(num), "b"(x), "c"(y), "S"(bank)
        : "edx", "edi", "memory", "cc");
    return result;
}

static U8 framebuf[640 * 480];
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
    b[0] = 4; /* DeltaX */
    b[1] = 2; /* DeltaY */
    b[2] = 0; /* HotX */
    b[3] = 0; /* HotY */
    /* Line 0: NbBlock=2 (1 pair: skip 0, fill 4) */
    b[4] = 2; /* NbBlock (2 entries per pair) */
    b[5] = 0; /* NbTransp */
    b[6] = 4; /* NbFill */
    /* Line 1: NbBlock=2, skip 0, fill 4 */
    b[7] = 2;
    b[8] = 0;
    b[9] = 4;
}

static void test_cpp_basic(void) {
    build_mask_bank();
    setup_screen();
    ColMask = 0xFF;
    AffMask(0, 50, 50, g_bank);
    /* All 8 pixels should be filled with ColMask=0xFF */
    ASSERT_EQ_UINT(0xFF, framebuf[50 * 640 + 50]);
    ASSERT_EQ_UINT(0xFF, framebuf[50 * 640 + 53]);
    ASSERT_EQ_UINT(0xFF, framebuf[51 * 640 + 50]);
    /* Outside the mask area should be 0 */
    ASSERT_EQ_UINT(0, framebuf[49 * 640 + 50]);
}

static void test_asm_equiv(void) {
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];
    build_mask_bank();

    setup_screen();
    ColMask = 0xAB;
    AffMask(0, 100, 100, g_bank);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    ColMask = 0xAB;
    asm_ColMask = 0xAB;
    call_asm_AffMask(0, 100, 100, g_bank);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), "AffMask");
}

static void test_random_positions(void) {
    U8 cpp_buf[640 * 480];
    U8 asm_buf[640 * 480];
    U32 rng = 0xBEEF1234;
    int prev = test_failures;
    build_mask_bank();
    for (int i = 0; i < 20 && test_failures == prev; i++) {
        rng = rng * 1103515245u + 12345u;
        S32 mx = (S32)((rng >> 16) % 600);
        rng = rng * 1103515245u + 12345u;
        S32 my = (S32)((rng >> 16) % 460);
        rng = rng * 1103515245u + 12345u;
        U8 col = (U8)(rng >> 16);

        setup_screen();
        ColMask = col;
        AffMask(0, mx, my, g_bank);
        memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

        setup_screen();
        asm_ColMask = col;
        call_asm_AffMask(0, mx, my, g_bank);
        memcpy(asm_buf, framebuf, sizeof(asm_buf));

        char label[64];
        snprintf(label, sizeof(label), "AffMask random #%d (%d,%d) col=0x%02X", i, mx, my, col);
        ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), label);
    }
}

int main(void) {
    RUN_TEST(test_cpp_basic);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_random_positions);
    TEST_SUMMARY();
    return test_failures != 0;
}
