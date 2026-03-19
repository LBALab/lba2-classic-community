/* Test: ClearBox / SetClearColor — clear rectangular region */
#include "test_harness.h"

#include <SVGA/CLRBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

/* ASM ClearBoxF is C-adapted (PROC with stack params) — callable directly. */
extern "C" void asm_ClearBoxF(void *dst, U32 *TabOffDst, T_BOX *box);
extern "C" U64 asm_ClearColor;

/* ASM SetClearColor uses Watcom register convention: parm [eax] */
extern "C" void asm_SetClearColor(void);
static void call_asm_SetClearColor(U32 color) {
    __asm__ __volatile__(
        "call asm_SetClearColor\n\t"
        :
    : "a"(color)
    : "memory", "ecx", "edx");
}

extern U64 ClearColor;

static U8 framebuf[640 * 480];
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

static void setup_screen(void)
{
    memset(framebuf, 0, sizeof(framebuf));
    Log = framebuf;
    ModeDesiredX = 640;
    ModeDesiredY = 480;
    for (U32 i = 0; i < 480; i++) TabOffLine[i] = i * 640;
    ClipXMin = 0; ClipYMin = 0; ClipXMax = 639; ClipYMax = 479;
}

static void assert_clearbox_case(const char *label, U32 color, const T_BOX *box)
{
    static U8 cpp_buf[640 * 480];
    static U8 asm_buf[640 * 480];

    setup_screen();
    memset(framebuf, 0xAA, sizeof(framebuf));
    SetClearColor(color);
    ClearBox(framebuf, TabOffLine, (T_BOX *)box);
    memcpy(cpp_buf, framebuf, sizeof(cpp_buf));

    setup_screen();
    memset(framebuf, 0xAA, sizeof(framebuf));
    call_asm_SetClearColor(color);
    asm_ClearBoxF(framebuf, TabOffLine, (T_BOX *)box);
    memcpy(asm_buf, framebuf, sizeof(asm_buf));

    ASSERT_ASM_CPP_EQ_INT(asm_ClearColor, ClearColor, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_buf, cpp_buf, sizeof(cpp_buf), label);
}

static void test_equivalence(void)
{
    T_BOX box;

    box.x0 = 0; box.y0 = 10; box.x1 = 16; box.y1 = 20; box.pBoxNext = NULL;
    assert_clearbox_case("ClearBox fixed width16", 0x00, &box);

    box.x0 = 8; box.y0 = 5; box.x1 = 32; box.y1 = 30; box.pBoxNext = NULL;
    assert_clearbox_case("ClearBox fixed width24", 0x42, &box);

    box.x0 = 24; box.y0 = 40; box.x1 = 32; box.y1 = 60; box.pBoxNext = NULL;
    assert_clearbox_case("ClearBox fixed width8", 0x7F, &box);
}

static void test_random_equivalence(void)
{
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 100; ++i) {
        T_BOX box;
        S32 width_units = (S32)(rng_next() % 8u) + 1;
        S32 width = width_units * 8;
        S32 x0 = (S32)(rng_next() % (640u - (U32)width));
        S32 y0 = (S32)(rng_next() % 470u);
        S32 height = (S32)(rng_next() % (480u - (U32)y0)) + 1;
        char label[64];

        box.x0 = (S16)x0;
        box.y0 = (S16)y0;
        box.x1 = (S16)(x0 + width);
        box.y1 = (S16)(y0 + height);
        box.pBoxNext = NULL;

        snprintf(label, sizeof(label), "ClearBox rand %d", i);
        assert_clearbox_case(label, rng_next() & 0xFFu, &box);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
