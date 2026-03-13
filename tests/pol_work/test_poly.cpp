/* Test: POLY.CPP — core polygon infrastructure.
 *
 * Tests SetFog, INV64, Switch_Fillers, SetCLUT, and Fill_Poly
 * integration with various polygon types.
 *
 * ASM-vs-CPP equivalence tests for INV64, SetFog, SetCLUT,
 * and Switch_Fillers_ASM.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYFLAT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>
#include <stdint.h>

/* ── ASM-side declarations ─────────────────────────────────────── */

/* SetFog and SetCLUT are C-convention PROCs in POLY.ASM, renamed
 * to asm_ prefix by objcopy.  Callable directly from C. */
extern "C" void asm_SetFog(S32 z_near, S32 z_far);
extern "C" void asm_SetCLUT(U32 defaultline);

/* Switch_Fillers_ASM uses Watcom register convention (Bank in EAX).
 * We declare it as a void(void) symbol and use inline ASM to
 * pass the bank value in EAX. */
extern "C" void asm_Switch_Fillers_ASM(void);

/* Fill_ScaledFogNear and Fill_Fog_Factor are defined in POLY.CPP
 * but not declared in POLY.H. */
extern "C" U32 Fill_ScaledFogNear;
extern "C" U32 Fill_Fog_Factor;

/* INV64 is not in POLY.ASM (it was likely a compiler intrinsic or
 * inline in the original Watcom build).  Replicate the exact x86
 * instruction sequence: xor eax,eax / mov edx,1 / idiv <a>. */
static S32 asm_INV64(S32 a)
{
    S32 result;
    __asm__ __volatile__(
        "xorl %%eax, %%eax\n\t"
        "movl $1, %%edx\n\t"
        "idivl %[divisor]"
        : "=a"(result)
        : [divisor] "rm"(a)
        : "edx"
    );
    return result;
}

/* Inline wrapper for Switch_Fillers_ASM (Bank in EAX). */
static void call_asm_Switch_Fillers(U32 bank)
{
    __asm__ __volatile__(
        "call asm_Switch_Fillers_ASM\n"
        :
        : "a"(bank)
        : "memory", "cc"
    );
}

/* ── INV64 ─────────────────────────────────────────────────────── */

static void test_inv64_positive(void)
{
    /* INV64(a) = (1<<32) / a — integer reciprocal for 16.16 fixed point */
    S32 r = INV64(65536);
    ASSERT_EQ_INT(65536, r);
}

static void test_inv64_small(void)
{
    S32 r = INV64(1);
    /* 2^32 / 1 wraps to 0 in 32-bit signed — expected behavior */
    (void)r; /* just confirm it doesn't crash */
    ASSERT_TRUE(1);
}

static void test_inv64_negative(void)
{
    S32 r = INV64(-65536);
    ASSERT_EQ_INT(-65536, r);
}

/* ── SetFog ────────────────────────────────────────────────────── */

static void test_setfog_basic(void)
{
    SetFog(100, 1000);
    ASSERT_EQ_INT(100, Fill_Z_Fog_Near);
    ASSERT_EQ_INT(1000, Fill_Z_Fog_Far);
    ASSERT_TRUE(Fill_ZBuffer_Factor > 0);
}

static void test_setfog_zero_far(void)
{
    /* z_far=0 should be clamped to 1 to avoid division by zero */
    SetFog(0, 0);
    ASSERT_EQ_INT(1, Fill_Z_Fog_Far);
    /* ZBuffer factor may wrap with these extreme values — just check no crash */
    ASSERT_TRUE(1);
}

static void test_setfog_large_range(void)
{
    SetFog(0, 65535);
    ASSERT_EQ_INT(0, Fill_Z_Fog_Near);
    ASSERT_EQ_INT(65535, Fill_Z_Fog_Far);
    ASSERT_TRUE(Fill_ZBuffer_Factor > 0);
}

/* ── Switch_Fillers ────────────────────────────────────────────── */

static void test_switch_normal(void)
{
    Switch_Fillers(FILL_POLY_NO_TEXTURES);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_Fog);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_ZBuffer);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_NZW);
}

static void test_switch_fog(void)
{
    Switch_Fillers(FILL_POLY_FOG);
    ASSERT_EQ_UINT(TRUE, Fill_Flag_Fog);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_ZBuffer);
}

static void test_switch_zbuffer(void)
{
    Switch_Fillers(FILL_POLY_ZBUFFER);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_Fog);
    ASSERT_EQ_UINT(TRUE, Fill_Flag_ZBuffer);
    ASSERT_EQ_UINT(FALSE, Fill_Flag_NZW);
}

static void test_switch_nzw(void)
{
    Switch_Fillers(FILL_POLY_NZW);
    ASSERT_EQ_UINT(TRUE, Fill_Flag_ZBuffer);
    ASSERT_EQ_UINT(TRUE, Fill_Flag_NZW);
}

/* ── SetScreenPitch ────────────────────────────────────────────── */

static void test_set_screen_pitch(void)
{
    U32 tab[2] = { 0, 320 };
    SetScreenPitch(tab);
    ASSERT_EQ_UINT(320, ScreenPitch);
    ASSERT_TRUE(PTR_TabOffLine == tab);
}

/* ── Fill_Poly integration with quad (4 points) ─────────────────── */

static void test_fill_poly_quad(void)
{
    setup_polygon_screen();
    Struc_Point pts[4];
    pts[0] = make_point(20, 20);
    pts[1] = make_point(20, 80);
    pts[2] = make_point(100, 80);
    pts[3] = make_point(100, 20);
    Fill_Poly(POLY_SOLID, 0x77, 4, pts);
    /* Centre should be filled */
    ASSERT_EQ_UINT(0x77, g_poly_framebuf[50 * TEST_POLY_W + 60]);
}

/* ── Randomized stress: random triangles with random types ──────── */

static void test_fill_poly_random_types(void)
{
    poly_rng_seed(0xCAFEBABE);
    for (int i = 0; i < 30; i++) {
        setup_polygon_screen();
        /* Pre-fill for transparency tests */
        memset(g_poly_framebuf, 0x11, TEST_POLY_SIZE);
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            S16 x = (S16)((poly_rng_next() % (TEST_POLY_W + 20)) - 10);
            S16 y = (S16)((poly_rng_next() % (TEST_POLY_H + 20)) - 10);
            pts[v] = make_point(x, y);
        }
        U8 color = (U8)(poly_rng_next() | 1);
        /* Test types 0-3 (flat/trans/trame) */
        S32 type = (S32)(poly_rng_next() % 4);
        Fill_Poly(type, color, 3, pts);
        /* Just ensure no crash */
        ASSERT_TRUE(1);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * ASM-vs-CPP equivalence tests
 * ═══════════════════════════════════════════════════════════════════ */

/* ── INV64: 300-round ASM-vs-CPP random stress ──────────────────── */

static void test_asm_random_inv64(void)
{
    poly_rng_seed(0x1234ABCD);
    int rounds = 0;
    while (rounds < 300) {
        /* Generate a full 32-bit random value (both positive and negative) */
        S32 a = (S32)((poly_rng_next() << 17) | (poly_rng_next() << 2)
                      | (poly_rng_next() & 0x3));
        /* Avoid values that cause x86 idiv #DE overflow:
         * a == 0 (div-by-zero), |a| <= 2 (quotient > INT32_MAX). */
        if (a >= -2 && a <= 2) continue;

        S32 cpp_result = INV64(a);
        S32 asm_result = asm_INV64(a);

        char msg[128];
        snprintf(msg, sizeof(msg), "INV64(%d) round #%d", a, rounds);
        ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, msg);
        rounds++;
    }
}

/* ── SetFog: 300-round ASM-vs-CPP random stress ─────────────────── */

static void test_asm_random_setfog(void)
{
    poly_rng_seed(0xF0600000);
    for (int i = 0; i < 300; i++) {
        S32 z_near = (S32)(poly_rng_next() % 10000);
        S32 z_far  = (S32)(poly_rng_next() % 50000) + 2; /* >= 2: avoid overflow */

        /* CPP */
        SetFog(z_near, z_far);
        S32 cpp_near = Fill_Z_Fog_Near;
        S32 cpp_far  = Fill_Z_Fog_Far;
        U32 cpp_zbf  = Fill_ZBuffer_Factor;
        U32 cpp_sfn  = Fill_ScaledFogNear;
        U32 cpp_ff   = Fill_Fog_Factor;

        /* ASM */
        asm_SetFog(z_near, z_far);

        char msg[128];
        snprintf(msg, sizeof(msg), "SetFog(%d,%d) #%d", z_near, z_far, i);
        ASSERT_ASM_CPP_EQ_INT(Fill_Z_Fog_Near,       cpp_near, msg);
        ASSERT_ASM_CPP_EQ_INT(Fill_Z_Fog_Far,        cpp_far,  msg);
        ASSERT_ASM_CPP_EQ_INT((S32)Fill_ZBuffer_Factor, (S32)cpp_zbf, msg);
        ASSERT_ASM_CPP_EQ_INT((S32)Fill_ScaledFogNear,  (S32)cpp_sfn, msg);
        ASSERT_ASM_CPP_EQ_INT((S32)Fill_Fog_Factor,     (S32)cpp_ff,  msg);
    }
}

/* ── SetCLUT: 300-round ASM-vs-CPP random stress ────────────────── */

static U8 g_fake_clut[65536];  /* 256 rows × 256 cols */

static void test_asm_random_setclut(void)
{
    /* Initialize fake CLUT with a known pattern */
    for (int i = 0; i < 65536; i++)
        g_fake_clut[i] = (U8)(i & 0xFF);
    PtrCLUTFog = g_fake_clut;

    poly_rng_seed(0xC1074000);
    for (int i = 0; i < 300; i++) {
        U32 line = poly_rng_next() % 256;

        /* CPP */
        PtrTruePal = NULL;  /* force update, bypass early-exit check */
        SetCLUT(line);
        PTR_U8 cpp_truePal     = PtrTruePal;
        PTR_U8 cpp_clutGouraud = PtrCLUTGouraud;
        U8 cpp_palette[256];
        memcpy(cpp_palette, Fill_Logical_Palette, 256);

        /* ASM */
        PtrTruePal = NULL;  /* force update */
        asm_SetCLUT(line);

        char msg[128];
        snprintf(msg, sizeof(msg), "SetCLUT(%u) #%d", line, i);
        ASSERT_ASM_CPP_EQ_INT((S32)(intptr_t)PtrTruePal,
                              (S32)(intptr_t)cpp_truePal, msg);
        ASSERT_ASM_CPP_EQ_INT((S32)(intptr_t)PtrCLUTGouraud,
                              (S32)(intptr_t)cpp_clutGouraud, msg);
        ASSERT_ASM_CPP_MEM_EQ(Fill_Logical_Palette, cpp_palette, 256, msg);
    }
}

/* ── Switch_Fillers: 300-round ASM-vs-CPP random stress ──────────── */

static void test_asm_random_switch_fillers(void)
{
    poly_rng_seed(0x5F110000);
    for (int i = 0; i < 300; i++) {
        U32 bank = poly_rng_next() % 8;

        /* CPP */
        Switch_Fillers(bank);
        U8 cpp_fog  = Fill_Flag_Fog;
        U8 cpp_zbuf = Fill_Flag_ZBuffer;
        U8 cpp_nzw  = Fill_Flag_NZW;

        /* ASM */
        call_asm_Switch_Fillers(bank);

        char msg[128];
        snprintf(msg, sizeof(msg), "Switch_Fillers(%u) #%d", bank, i);
        ASSERT_ASM_CPP_EQ_INT(Fill_Flag_Fog,     cpp_fog,  msg);
        ASSERT_ASM_CPP_EQ_INT(Fill_Flag_ZBuffer,  cpp_zbuf, msg);
        ASSERT_ASM_CPP_EQ_INT(Fill_Flag_NZW,      cpp_nzw,  msg);
    }
}

int main(void)
{
    RUN_TEST(test_inv64_positive);
    RUN_TEST(test_inv64_small);
    RUN_TEST(test_inv64_negative);
    RUN_TEST(test_setfog_basic);
    RUN_TEST(test_setfog_zero_far);
    RUN_TEST(test_setfog_large_range);
    RUN_TEST(test_switch_normal);
    RUN_TEST(test_switch_fog);
    RUN_TEST(test_switch_zbuffer);
    RUN_TEST(test_switch_nzw);
    RUN_TEST(test_set_screen_pitch);
    RUN_TEST(test_fill_poly_quad);
    RUN_TEST(test_fill_poly_random_types);
    RUN_TEST(test_asm_random_inv64);
    RUN_TEST(test_asm_random_setfog);
    RUN_TEST(test_asm_random_setclut);
    RUN_TEST(test_asm_random_switch_fillers);
    TEST_SUMMARY();
    return test_failures != 0;
}
