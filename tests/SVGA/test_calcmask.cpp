/* Test: CalcGraphMsk — convert graph brick data to mask format.
 * Pure computation — no rendering or screen globals needed. */
#include "test_harness.h"
#include <SVGA/CALCMASK.H>
#include <string.h>

extern "C" S32 asm_CalcGraphMsk(S32 numbrick, U8 *bankbrick, U8 *ptmask);

/* Synthetic brick bank with one simple brick.
 * Format: U32 offset[1] + brick header + line data.
 * Line data: NbBlock, then per block: opcode byte + data.
 *   Opcode bits 6-7: 00=jump(transparent), 01=copy, 10=repeat
 *   Opcode bits 0-5: count-1 */
static U8 g_brick[512];
static U8 g_mask_cpp[512];
static U8 g_mask_asm[512];
static const U8 k_simple_mask_expected[] = {8, 2, 0, 0, 1, 10, 1, 3};

static void build_simple_brick(void) {
    memset(g_brick, 0, sizeof(g_brick));
    U32 *offsets = (U32 *)g_brick;
    offsets[0] = 4; /* brick 0 at offset 4 */

    U8 *b = g_brick + 4;
    b[0] = 8; /* DeltaX = 8 pixels wide */
    b[1] = 2; /* DeltaY = 2 lines */
    b[2] = 0; /* HotX */
    b[3] = 0; /* HotY */

    /* Brick format: NbBlock (2=one pair), NbTransp, NbFill, pixel data.
     * Line 0: 1 pair (skip 0, fill 8), 8 pixels of data */
    U8 *line = b + 4;
    line[0] = 2; /* NbBlock = 2 (one pair) */
    line[1] = 0; /* NbTransp = 0 */
    line[2] = 8; /* NbFill = 8 */
    line[3] = 1;
    line[4] = 2;
    line[5] = 3;
    line[6] = 4;
    line[7] = 5;
    line[8] = 6;
    line[9] = 7;
    line[10] = 8;

    /* Line 1: 2 pairs: (skip 3) + (fill 5) */
    U8 *line2 = line + 11;
    line2[0] = 4; /* NbBlock = 4 (two pairs) */
    line2[1] = 3; /* NbTransp = 3 */
    line2[2] = 0; /* NbFill = 0 */
    line2[3] = 0; /* NbTransp = 0 */
    line2[4] = 5; /* NbFill = 5 */
    line2[5] = 10;
    line2[6] = 11;
    line2[7] = 12;
    line2[8] = 13;
    line2[9] = 14;
}

/* ── LCG random number generator ────────────────────────────────────── */
static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

/* Build a random valid brick in g_brick.
 * Keeps sizes modest to stay well within the 512-byte buffer. */
static void build_random_brick(void) {
    memset(g_brick, 0, sizeof(g_brick));
    U32 *offsets = (U32 *)g_brick;
    offsets[0] = 4; /* brick 0 at offset 4 */

    U8 *b = g_brick + 4;
    U8 deltaX = (U8)(4 + rng_next() % 12); /* 4-15 pixels wide */
    U8 deltaY = (U8)(1 + rng_next() % 3);  /* 1-3 lines */
    b[0] = deltaX;
    b[1] = deltaY;
    b[2] = 0; /* HotX */
    b[3] = 0; /* HotY */

    U8 *ptr = b + 4;
    for (int line = 0; line < deltaY; line++) {
        U32 pixelsRemaining = deltaX;
        U8 *nbBlockPtr = ptr++;
        U8 nbBlock = 0;

        while (pixelsRemaining > 0) {
            U32 maxCount = pixelsRemaining < 16 ? pixelsRemaining : 16;
            U32 count = 1 + rng_next() % maxCount;
            if (count > 64)
                count = 64; /* max 6 bits */

            U32 type = rng_next() % 3; /* 0=jump, 1=copy, 2=repeat */
            U8 opcode = (U8)((type << 6) | ((count - 1) & 0x3F));
            *ptr++ = opcode;

            if (type == 1) { /* copy: count data bytes */
                for (U32 i = 0; i < count; i++)
                    *ptr++ = (U8)(rng_next() & 0xFF);
            } else if (type == 2) { /* repeat: 1 data byte */
                *ptr++ = (U8)(rng_next() & 0xFF);
            }
            /* type 0 = jump: no data bytes */

            pixelsRemaining -= count;
            nbBlock++;
        }
        *nbBlockPtr = nbBlock;
    }
}

static void test_cpp_basic(void) {
    build_simple_brick();
    memset(g_mask_cpp, 0, sizeof(g_mask_cpp));
    S32 size = CalcGraphMsk(0, g_brick, g_mask_cpp);

    ASSERT_EQ_INT((S32)sizeof(k_simple_mask_expected), size);
    ASSERT_MEM_EQ(k_simple_mask_expected, g_mask_cpp, sizeof(k_simple_mask_expected));
}

static void test_asm_equiv(void) {
    build_simple_brick();
    memset(g_mask_cpp, 0, sizeof(g_mask_cpp));
    memset(g_mask_asm, 0, sizeof(g_mask_asm));

    S32 cpp_size = CalcGraphMsk(0, g_brick, g_mask_cpp);
    S32 asm_size = asm_CalcGraphMsk(0, g_brick, g_mask_asm);

    ASSERT_EQ_INT(cpp_size, asm_size);
    ASSERT_ASM_CPP_MEM_EQ(g_mask_asm, g_mask_cpp, (U32)cpp_size, "CalcGraphMsk");
}

static void test_asm_random(void) {
    rng_seed(0xDEADBEEF);
    for (int i = 0; i < 300; i++) {
        build_random_brick();
        memset(g_mask_cpp, 0, sizeof(g_mask_cpp));
        memset(g_mask_asm, 0, sizeof(g_mask_asm));

        S32 cpp_size = CalcGraphMsk(0, g_brick, g_mask_cpp);
        S32 asm_size = asm_CalcGraphMsk(0, g_brick, g_mask_asm);

        char msg[128];
        snprintf(msg, sizeof(msg), "CalcGraphMsk random #%d", i);
        ASSERT_ASM_CPP_EQ_INT(asm_size, cpp_size, msg);
        if (cpp_size > 0 && cpp_size == asm_size) {
            ASSERT_ASM_CPP_MEM_EQ(g_mask_asm, g_mask_cpp, (U32)cpp_size, msg);
        }
    }
}

int main(void) {
    RUN_TEST(test_cpp_basic);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_asm_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
