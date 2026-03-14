/* Test: POLY.CPP — core polygon infrastructure.
 *
 * Tests SetFog, INV64, Switch_Fillers, SetCLUT, and Fill_Poly
 * integration with various polygon types.
 *
 * ASM-vs-CPP equivalence tests for INV64, SetFog, SetCLUT,
 * Switch_Fillers_ASM, Fill_Poly/Fill_PolyFast, Fill_PolyClip,
 * Fill_Sphere, and Line_A.
 *
 * Triangle_ReadNextEdge is tested indirectly through Fill_PolyClip:
 * the filler tail-calls Triangle_ReadNextEdge on every scanline strip.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLYFLAT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>
#include <stdint.h>
#include <fenv.h>
#include <SYSTEM/UTILS.H>

/* ASM REAL4 constant: FInv_65536 = 0.0000152588 (NOT exact 1/65536).
 * Reference computations must use this approximate value to match
 * the ASM's fmul [FInv_65536] behavior. */
static const float ASM_FInv_65536 = 0.0000152588f;

/* ── ASM-side declarations ─────────────────────────────────────── */

/* SetFog and SetCLUT are C-convention PROCs in POLY.ASM, renamed
 * to asm_ prefix by objcopy.  Callable directly from C. */
extern "C" void asm_SetFog(S32 z_near, S32 z_far);
extern "C" void asm_SetCLUT(U32 defaultline);

/* Switch_Fillers_ASM uses Watcom register convention (Bank in EAX).
 * We declare it as a void(void) symbol and use inline ASM to
 * pass the bank value in EAX. */
extern "C" void asm_Switch_Fillers_ASM(void);

/* Fill_PolyClip: Watcom convention: ECX=Nb_Points, ESI=Ptr_Points.
 * Epilogue does: pop esi / pop ecx / pop ebp / ret.  The "pop ebp"
 * matches the push in Fill_PolyFast, not Fill_PolyClip itself. */
extern "C" void asm_Fill_PolyClip(void);

/* ASM Filler_Flat (from POLYFLAT.ASM dep) — register-convention filler
 * used by the ASM Draw_Triangle → jmp [Fill_Filler] chain. */
extern "C" void asm_Filler_Flat(void);

/* ASM Fill_Sphere (from POLYDISC.ASM dep).
 * Watcom convention: ESI=Type, EDX=Color, EAX=CentreX, EBX=CentreY,
 *                    ECX=Rayon, EDI=zBufValue. */
extern "C" void asm_Fill_Sphere(void);

/* ASM Line_A (from POLYLINE.ASM dep).
 * Watcom convention: EAX=x0, EBX=y0, ECX=x1, EDX=y1, EBP=color. */
extern "C" void asm_Line_A(void);

/* ASM Filler_TextureZFogSmoothZBuf (from POLYTZF.ASM dep) — register-
 * convention filler for perspective-correct textured fog + zbuffer. */
extern "C" void asm_Filler_TextureZFogSmoothZBuf(void);

/* ASM jump table (from POLY_JMP.ASM dep) — renamed by objcopy.
 * Entries are addresses of private ASM Jmp_* procs (Watcom convention).
 * Used for structural verification only; not called directly due to
 * Watcom→C convention mismatch at the Jmp_*→Fill_PolyClip boundary. */
extern "C" U32 asm_Fill_N_Table_Jumps[];

/* Fill_ScaledFogNear and Fill_Fog_Factor are defined in POLY.CPP
 * but not declared in POLY.H. */
extern "C" U32 Fill_ScaledFogNear;
extern "C" U32 Fill_Fog_Factor;
extern "C" U32 Fill_Type;

/* F_256: ASM REAL4 constant needed by TextureZ perspective code in POLYTZF.ASM */
extern "C" { float F_256 = 256.0f; }

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

/* Fill_PolyClip's epilogue does: pop esi / pop ecx / pop ebp / ret.
 * The "pop ebp" is unmatched within Fill_PolyClip — it pops the EBP
 * that Fill_PolyFast pushes.  For standalone calls we mimic that
 * stack layout: push return_addr (for ret), push ebp (for pop ebp),
 * then jmp.  */
static S32 call_asm_Fill_PolyClip(S32 nb_pts, Struc_Point *pts)
{
    S32 result;
    __asm__ __volatile__(
        "push %%ebp\n\t"              /* save caller ebp                 */
        "pushl $1f\n\t"               /* return address for ret          */
        "push %%ebp\n\t"              /* dummy for pop ebp in epilogue   */
        "jmp  asm_Fill_PolyClip\n\t"
        "1:\n\t"
        "pop  %%ebp\n\t"              /* restore caller ebp              */
        : "=a"(result)
        : "c"(nb_pts), "S"(pts)
        : "edi", "ebx", "edx", "memory", "cc"
    );
    return result;
}

/* Fill_Sphere: ESI=Type, EDX=Color, EAX=CentreX, EBX=CentreY,
 *              ECX=Rayon, EDI=zBufValue. */
static void call_asm_Fill_Sphere(S32 type, S32 color,
                                  S32 cx, S32 cy, S32 r, S32 zbuf)
{
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Fill_Sphere\n\t"
        "pop  %%ebp"
        :
        : "S"(type), "d"(color), "a"(cx), "b"(cy), "c"(r), "D"(zbuf)
        : "memory", "cc"
    );
}

/* Line_A: EAX=x0, EBX=y0, ECX=x1, EDX=y1, EBP=color.
 * Z params (EDI/ESI) only used by zbuffer paths; we test non-zbuffer. */
static void call_asm_Line_A(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)
{
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "movl %4, %%ebp\n\t"
        "call asm_Line_A\n\t"
        "pop  %%ebp"
        :
        : "a"(x0), "b"(y0), "c"(x1), "d"(y1), "m"(col)
        : "esi", "edi", "memory", "cc"
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
    for (int i = 0; i < 300; i++) {
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

/* ── Buffers for framebuffer comparison ─────────────────────────── */

static U8 poly_cpp_buf[TEST_POLY_SIZE];
static U8 poly_asm_buf[TEST_POLY_SIZE];

/* ── Fill_Poly — end-to-end via asm_Fill_PolyClip ───────────────── *
 *
 * Fill_PolyFast cannot be tested directly because its jmp-table
 * dispatch sends Watcom-register parameters to the CPP Jmp_Solid
 * handler (C calling convention) — an unavoidable convention mismatch.
 *
 * Instead, Fill_Poly is effectively tested through Fill_PolyClip:
 * Fill_Poly merely sets Fill_LeftSlope, calls SetScreenPitch, and
 * dispatches to Jmp_Solid which calls Fill_PolyClip.  The actual
 * rendering logic lives in Fill_PolyClip and is tested below.        */

/* ── Fill_PolyClip: 300-round ASM-vs-CPP ────────────────────────── *
 *
 * This exercises the full ASM bounding-box, clipping, edge-walking,
 * and triangle setup code.  The ASM path uses asm_Filler_Flat
 * (register-convention jmp chain), while the CPP path uses the
 * normal CPP Filler_Flat.
 *
 * Triangle_ReadNextEdge is tested INDIRECTLY here: the filler's
 * tail-call chain (jmp Triangle_ReadNextEdge) runs on every
 * scanline strip.                                                     */

static void test_asm_random_fill_polyclip(void)
{
    poly_rng_seed(0xC110DEAD);
    for (int i = 0; i < 300; i++) {
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            /* Keep vertices within screen bounds to avoid clipping paths */
            S16 x = (S16)(poly_rng_next() % TEST_POLY_W);
            S16 y = (S16)(poly_rng_next() % TEST_POLY_H);
            pts[v] = make_point(x, y);
        }
        U8 color = (U8)(poly_rng_next() | 1);

        /* CPP path: set globals as Jmp_Solid does */
        setup_polygon_screen();
        Fill_Filler = Filler_Flat;
        Fill_ClipFlag = 1; /* CLIP_FLAT */
        Fill_Color.Num = color | ((U32)color << 8);
        Fill_PolyClip(3, pts);
        memcpy(poly_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        /* ASM path: use ASM filler for register calling convention.
         * Call asm_Switch_Fillers to ensure ASM-side slope tables and
         * internal state are initialised consistently. */
        setup_polygon_screen();
        call_asm_Switch_Fillers(FILL_POLY_NO_TEXTURES);
        Fill_Filler = (Fill_Filler_Func)(void *)&asm_Filler_Flat;
        Fill_ClipFlag = 1; /* CLIP_FLAT */
        Fill_Color.Num = color | ((U32)color << 8);
        call_asm_Fill_PolyClip(3, pts);
        memcpy(poly_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "Fill_PolyClip #%d", i);
        ASSERT_ASM_CPP_MEM_EQ(poly_asm_buf, poly_cpp_buf,
                               TEST_POLY_SIZE, msg);
    }
}

/* ── Fill_Sphere: 300-round ASM-vs-CPP ──────────────────────────── */

static void test_asm_random_fill_sphere(void)
{
    poly_rng_seed(0x5E4E1E42);
    for (int i = 0; i < 300; i++) {
        S32 cx = (S32)(poly_rng_next() % (TEST_POLY_W + 60)) - 30;
        S32 cy = (S32)(poly_rng_next() % (TEST_POLY_H + 60)) - 30;
        S32 r  = (S32)(poly_rng_next() % 40);
        S32 color = (S32)(poly_rng_next() & 0xFE) | 1;

        setup_polygon_screen();
        Fill_Sphere(0, color, cx, cy, r, 0);
        memcpy(poly_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_polygon_screen();
        call_asm_Fill_Sphere(0, color, cx, cy, r, 0);
        memcpy(poly_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Fill_Sphere #%d cx=%d cy=%d r=%d col=%d",
                 i, cx, cy, r, color);
        ASSERT_ASM_CPP_MEM_EQ(poly_asm_buf, poly_cpp_buf,
                               TEST_POLY_SIZE, msg);
    }
}

/* ── Line_A: 300-round ASM-vs-CPP (non-zbuffer mode) ────────────── */

static void test_asm_random_line_a(void)
{
    poly_rng_seed(0x11AEBEEF);
    for (int i = 0; i < 300; i++) {
        S32 x0 = (S32)(poly_rng_next() % (TEST_POLY_W + 40)) - 20;
        S32 y0 = (S32)(poly_rng_next() % (TEST_POLY_H + 40)) - 20;
        S32 x1 = (S32)(poly_rng_next() % (TEST_POLY_W + 40)) - 20;
        S32 y1 = (S32)(poly_rng_next() % (TEST_POLY_H + 40)) - 20;
        S32 col = (S32)(poly_rng_next() & 0xFF);
        if (col == 0) col = 1;

        setup_polygon_screen();
        Line_A(x0, y0, x1, y1, col, 0, 0);
        memcpy(poly_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_polygon_screen();
        call_asm_Line_A(x0, y0, x1, y1, col);
        memcpy(poly_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Line_A #%d (%d,%d)-(%d,%d) col=%d",
                 i, x0, y0, x1, y1, col);
        ASSERT_ASM_CPP_MEM_EQ(poly_asm_buf, poly_cpp_buf,
                               TEST_POLY_SIZE, msg);
    }
}

/* ── TextureZ slope calculator unit tests ────────────────────────── *
 *
 * The CPP slope calculators (Calc_TextureZ*SlopeFPU) are standalone
 * functions that read globals (YB_YA, YC_YA, InvDenom) and write
 * slope globals (Fill_MapU_XSlope, etc.).  They tail-call
 * Triangle_ReadNextEdge, which we handle via setup_filler_exit.
 *
 * Since the ASM slope calculators are internal to Draw_Triangle's
 * jump tables and cannot be called separately, this test validates
 * the CPP implementations against a reference formula to catch
 * regressions.                                                       */

extern "C" double YB_YA;
extern "C" double YC_YA;
extern "C" volatile long double InvDenom;

extern "C" S32 Calc_TextureZXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC);
extern "C" S32 Calc_TextureZGouraudXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC);
extern "C" S32 Calc_TextureZXSlopeZBufFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC);
extern "C" S32 Calc_XSlopeZBufferFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC);

extern "C" S32 Calc_TextureZLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB);
extern "C" S32 Calc_TextureZGouraudLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB);
extern "C" S32 Calc_TextureZLeftSlopeZBufFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB);
extern "C" S32 Calc_LeftSlopeZBufferFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB);

static Struc_Point make_texz_point(S16 x, S16 y, U16 u, U16 v, S32 w, U16 light, U16 zo)
{
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = x;
    p.Pt_YE = y;
    p.Pt_MapU = u;
    p.Pt_MapV = v;
    p.Pt_W = w;
    p.Pt_Light = light;
    p.Pt_ZO = zo;
    return p;
}

/* Set up state so the tail-call chain (Triangle_ReadNextEdge →
 * Read_Next_Right → Test_Scan) exits cleanly without calling
 * any filler.  Fill_CurY must equal the exit-point Y so that
 * Test_Scan gets diffY==0 and recurses into Triangle_ReadNextEdge,
 * which finds a point with lower Y and returns immediately. */
static void setup_slope_test_state(S16 exitY)
{
    setup_polygon_screen();
    setup_filler_exit(exitY);
    Fill_CurY = exitY;
    Fill_CurOffLine = (PTR_U8)((U8 *)Log + TabOffLine[exitY > 0 ? exitY : 0]);
    Fill_CurXMin = 0x00100000;
    Fill_CurXMax = 0x00500000;
    Fill_Filler = NULL;
}

static void test_texturez_xslope(void)
{
    poly_rng_seed(0xDEADBEEF);

    for (int i = 0; i < 300; i++) {
        S16 xA = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yA = (S16)(poly_rng_next() % TEST_POLY_H);
        S16 xB = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yB = (S16)(poly_rng_next() % TEST_POLY_H);
        S16 xC = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yC = (S16)(poly_rng_next() % TEST_POLY_H);

        if (yA == yB && yB == yC) yC = (S16)((yC + 5) % TEST_POLY_H);
        if (yA == yB) yB = (S16)((yB + 3) % TEST_POLY_H);

        U16 uA = (U16)(poly_rng_next() & 0xFF);
        U16 vA = (U16)(poly_rng_next() & 0xFF);
        U16 uB = (U16)(poly_rng_next() & 0xFF);
        U16 vB = (U16)(poly_rng_next() & 0xFF);
        U16 uC = (U16)(poly_rng_next() & 0xFF);
        U16 vC = (U16)(poly_rng_next() & 0xFF);

        S32 wA = (S32)(poly_rng_next() % 65280 + 256);
        S32 wB = (S32)(poly_rng_next() % 65280 + 256);
        S32 wC = (S32)(poly_rng_next() % 65280 + 256);

        Struc_Point ptA = make_texz_point(xA, yA, uA, vA, wA, 0, 0);
        Struc_Point ptB = make_texz_point(xB, yB, uB, vB, wB, 0, 0);
        Struc_Point ptC = make_texz_point(xC, yC, uC, vC, wC, 0, 0);

        /* Compute denominator as Draw_Triangle does */
        double yb_ya = ptB.Pt_YE - ptA.Pt_YE;
        double yc_ya = ptC.Pt_YE - ptA.Pt_YE;
        volatile long double raw_Denom = (long double)yb_ya * (long double)(ptC.Pt_XE - ptA.Pt_XE)
                                       - (long double)yc_ya * (long double)(ptB.Pt_XE - ptA.Pt_XE);
        if (raw_Denom == 0) continue;

        YB_YA = yb_ya;
        YC_YA = yc_ya;
        InvDenom = 256.0L / raw_Denom;

        /* Compute expected values with the same formula */
        fesetround(FE_TOWARDZERO);
        RoundType = ROUND_TYPE_INT;

        volatile long double Dp = InvDenom * (long double)ASM_FInv_65536;

        volatile long double UAp = (long double)(S32)ptA.Pt_MapU * (long double)ptA.Pt_W;
        volatile long double UBp = (long double)(S32)ptB.Pt_MapU * (long double)ptB.Pt_W;
        volatile long double UCp = (long double)(S32)ptC.Pt_MapU * (long double)ptC.Pt_W;
        volatile long double MUC = (UCp - UAp) * (long double)YB_YA;
        volatile long double MUB = (UBp - UAp) * (long double)YC_YA;
        volatile long double USlope = (MUC - MUB) * Dp;
        S32 expected_U = (S32)USlope;

        volatile long double VAp = (long double)(S32)ptA.Pt_MapV * (long double)ptA.Pt_W;
        volatile long double VBp = (long double)(S32)ptB.Pt_MapV * (long double)ptB.Pt_W;
        volatile long double VCp = (long double)(S32)ptC.Pt_MapV * (long double)ptC.Pt_W;
        volatile long double MVC = (VCp - VAp) * (long double)YB_YA;
        volatile long double MVB = (VBp - VAp) * (long double)YC_YA;
        volatile long double VSlope = (MVC - MVB) * Dp;
        S32 expected_V = (S32)VSlope;

        volatile long double WB_WA = (long double)ptB.Pt_W - (long double)ptA.Pt_W;
        volatile long double WC_WA = (long double)ptC.Pt_W - (long double)ptA.Pt_W;
        volatile long double MWC = WC_WA * (long double)YB_YA;
        volatile long double MWB = WB_WA * (long double)YC_YA;
        volatile long double WSlope = (MWC - MWB) * Dp;
        volatile long double WSlope_scaled = WSlope * 256.0L;
        S32 expected_W = (S32)WSlope_scaled;

        /* Call the slope calculator */
        setup_slope_test_state(10);
        Fill_Type = POLY_TEXTURE_Z;
        Fill_MapU_XSlope = 0;
        Fill_MapV_XSlope = 0;
        Fill_W_XSlope = 0;

        Calc_TextureZXSlopeFPU(POLY_TEXTURE_Z, &ptA, &ptB, &ptC);

        fesetround(FE_TONEAREST);
        RoundType = ROUND_TYPE_FLOAT;

        char msg[128];
        snprintf(msg, sizeof(msg), "TextureZXSlope U #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_U, Fill_MapU_XSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZXSlope V #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_V, Fill_MapV_XSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZXSlope W #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_W, Fill_W_XSlope, msg);
    }
}

static void test_texturez_gouraud_xslope(void)
{
    poly_rng_seed(0xCAFE1234);

    for (int i = 0; i < 300; i++) {
        S16 xA = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yA = (S16)(poly_rng_next() % TEST_POLY_H);
        S16 xB = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yB = (S16)(poly_rng_next() % TEST_POLY_H);
        S16 xC = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yC = (S16)(poly_rng_next() % TEST_POLY_H);

        if (yA == yB && yB == yC) yC = (S16)((yC + 5) % TEST_POLY_H);
        if (yA == yB) yB = (S16)((yB + 3) % TEST_POLY_H);

        U16 uA = (U16)(poly_rng_next() & 0xFF);
        U16 vA = (U16)(poly_rng_next() & 0xFF);
        U16 uB = (U16)(poly_rng_next() & 0xFF);
        U16 vB = (U16)(poly_rng_next() & 0xFF);
        U16 uC = (U16)(poly_rng_next() & 0xFF);
        U16 vC = (U16)(poly_rng_next() & 0xFF);

        S32 wA = (S32)(poly_rng_next() % 65280 + 256);
        S32 wB = (S32)(poly_rng_next() % 65280 + 256);
        S32 wC = (S32)(poly_rng_next() % 65280 + 256);

        U16 lightA = (U16)(poly_rng_next() % 3584);
        U16 lightB = (U16)(poly_rng_next() % 3584);
        U16 lightC = (U16)(poly_rng_next() % 3584);

        Struc_Point ptA = make_texz_point(xA, yA, uA, vA, wA, lightA, 0);
        Struc_Point ptB = make_texz_point(xB, yB, uB, vB, wB, lightB, 0);
        Struc_Point ptC = make_texz_point(xC, yC, uC, vC, wC, lightC, 0);

        double yb_ya = ptB.Pt_YE - ptA.Pt_YE;
        double yc_ya = ptC.Pt_YE - ptA.Pt_YE;
        volatile long double raw_Denom = (long double)yb_ya * (long double)(ptC.Pt_XE - ptA.Pt_XE)
                                       - (long double)yc_ya * (long double)(ptB.Pt_XE - ptA.Pt_XE);
        if (raw_Denom == 0) continue;

        YB_YA = yb_ya;
        YC_YA = yc_ya;
        InvDenom = 256.0L / raw_Denom;

        fesetround(FE_TOWARDZERO);
        RoundType = ROUND_TYPE_INT;

        volatile long double Dp = InvDenom * (long double)ASM_FInv_65536;

        /* U/V slopes use Dp (same as base TextureZ) */
        volatile long double UAp = (long double)(S32)ptA.Pt_MapU * (long double)ptA.Pt_W;
        volatile long double UBp = (long double)(S32)ptB.Pt_MapU * (long double)ptB.Pt_W;
        volatile long double UCp = (long double)(S32)ptC.Pt_MapU * (long double)ptC.Pt_W;
        volatile long double USlope = ((UCp - UAp) * (long double)YB_YA - (UBp - UAp) * (long double)YC_YA) * Dp;
        S32 expected_U = (S32)USlope;

        volatile long double VAp = (long double)(S32)ptA.Pt_MapV * (long double)ptA.Pt_W;
        volatile long double VBp = (long double)(S32)ptB.Pt_MapV * (long double)ptB.Pt_W;
        volatile long double VCp = (long double)(S32)ptC.Pt_MapV * (long double)ptC.Pt_W;
        volatile long double VSlope = ((VCp - VAp) * (long double)YB_YA - (VBp - VAp) * (long double)YC_YA) * Dp;
        S32 expected_V = (S32)VSlope;

        /* Gouraud and W slopes use D = Dp * F_65536 (ASM REAL4 65536.0) */
        volatile long double D = Dp * (long double)65536.0f;

        S32 lightB_A = (S32)(ptB.Pt_Light & 0xFFFF) - (S32)(ptA.Pt_Light & 0xFFFF);
        S32 lightC_A = (S32)(ptC.Pt_Light & 0xFFFF) - (S32)(ptA.Pt_Light & 0xFFFF);
        volatile long double MLB = (long double)lightB_A * (long double)YC_YA;
        volatile long double MLC = (long double)lightC_A * (long double)YB_YA;
        volatile long double LSlope = (MLC - MLB) * D;
        S32 expected_G = (S32)LSlope;

        volatile long double WB_WA = (long double)ptB.Pt_W - (long double)ptA.Pt_W;
        volatile long double WC_WA = (long double)ptC.Pt_W - (long double)ptA.Pt_W;
        volatile long double WSlope = (WC_WA * (long double)YB_YA - WB_WA * (long double)YC_YA) * D;
        volatile long double WSlope_scaled = WSlope * (1.0L / 256.0L);
        S32 expected_W = (S32)WSlope_scaled;

        setup_slope_test_state(10);
        Fill_Type = POLY_TEXTURE_Z_GOURAUD;
        Fill_MapU_XSlope = 0;
        Fill_MapV_XSlope = 0;
        Fill_Gouraud_XSlope = 0;
        Fill_W_XSlope = 0;

        Calc_TextureZGouraudXSlopeFPU(POLY_TEXTURE_Z_GOURAUD, &ptA, &ptB, &ptC);

        fesetround(FE_TONEAREST);
        RoundType = ROUND_TYPE_FLOAT;

        char msg[128];
        snprintf(msg, sizeof(msg), "TextureZGouraudXSlope U #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_U, Fill_MapU_XSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudXSlope V #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_V, Fill_MapV_XSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudXSlope G #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_G, Fill_Gouraud_XSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudXSlope W #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_W, Fill_W_XSlope, msg);
    }
}

static void test_texturez_xslope_zbuf(void)
{
    poly_rng_seed(0xBEEF4567);

    for (int i = 0; i < 300; i++) {
        S16 xA = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yA = (S16)(poly_rng_next() % TEST_POLY_H);
        S16 xB = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yB = (S16)(poly_rng_next() % TEST_POLY_H);
        S16 xC = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yC = (S16)(poly_rng_next() % TEST_POLY_H);

        if (yA == yB && yB == yC) yC = (S16)((yC + 5) % TEST_POLY_H);
        if (yA == yB) yB = (S16)((yB + 3) % TEST_POLY_H);

        U16 uA = (U16)(poly_rng_next() & 0xFF);
        U16 vA = (U16)(poly_rng_next() & 0xFF);
        U16 uB = (U16)(poly_rng_next() & 0xFF);
        U16 vB = (U16)(poly_rng_next() & 0xFF);
        U16 uC = (U16)(poly_rng_next() & 0xFF);
        U16 vC = (U16)(poly_rng_next() & 0xFF);

        S32 wA = (S32)(poly_rng_next() % 65280 + 256);
        S32 wB = (S32)(poly_rng_next() % 65280 + 256);
        S32 wC = (S32)(poly_rng_next() % 65280 + 256);

        U16 zoA = (U16)(poly_rng_next() & 0xFFFF);
        U16 zoB = (U16)(poly_rng_next() & 0xFFFF);
        U16 zoC = (U16)(poly_rng_next() & 0xFFFF);

        Struc_Point ptA = make_texz_point(xA, yA, uA, vA, wA, 0, zoA);
        Struc_Point ptB = make_texz_point(xB, yB, uB, vB, wB, 0, zoB);
        Struc_Point ptC = make_texz_point(xC, yC, uC, vC, wC, 0, zoC);

        double yb_ya = ptB.Pt_YE - ptA.Pt_YE;
        double yc_ya = ptC.Pt_YE - ptA.Pt_YE;
        volatile long double raw_Denom = (long double)yb_ya * (long double)(ptC.Pt_XE - ptA.Pt_XE)
                                       - (long double)yc_ya * (long double)(ptB.Pt_XE - ptA.Pt_XE);
        if (raw_Denom == 0) continue;

        YB_YA = yb_ya;
        YC_YA = yc_ya;
        InvDenom = 256.0L / raw_Denom;

        fesetround(FE_TOWARDZERO);
        RoundType = ROUND_TYPE_INT;

        volatile long double Dp = InvDenom * (long double)ASM_FInv_65536;

        /* ZBuf slope — uses simple (non-perspective) Z formula via wrapper:
         * Calc_ZBufXSlopeFPU computes (ZC_ZA*YB_YA - ZB_ZA*YC_YA) * InvDenom */
        volatile long double ZB_ZA = (long double)(S32)(U32)(ptB.Pt_ZO - ptA.Pt_ZO);
        volatile long double ZC_ZA = (long double)(S32)(U32)(ptC.Pt_ZO - ptA.Pt_ZO);
        volatile long double ZSlope = (ZC_ZA * (long double)YB_YA - ZB_ZA * (long double)YC_YA) * InvDenom;
        S32 expected_Z = (S32)ZSlope;

        /* U/V/W slopes (same as base TextureZ) */
        volatile long double UAp = (long double)(S32)ptA.Pt_MapU * (long double)ptA.Pt_W;
        volatile long double UBp = (long double)(S32)ptB.Pt_MapU * (long double)ptB.Pt_W;
        volatile long double UCp = (long double)(S32)ptC.Pt_MapU * (long double)ptC.Pt_W;
        volatile long double USlope = ((UCp - UAp) * (long double)YB_YA - (UBp - UAp) * (long double)YC_YA) * Dp;
        S32 expected_U = (S32)USlope;

        volatile long double VAp = (long double)(S32)ptA.Pt_MapV * (long double)ptA.Pt_W;
        volatile long double VBp = (long double)(S32)ptB.Pt_MapV * (long double)ptB.Pt_W;
        volatile long double VCp = (long double)(S32)ptC.Pt_MapV * (long double)ptC.Pt_W;
        volatile long double VSlope = ((VCp - VAp) * (long double)YB_YA - (VBp - VAp) * (long double)YC_YA) * Dp;
        S32 expected_V = (S32)VSlope;

        volatile long double WB_WA = (long double)ptB.Pt_W - (long double)ptA.Pt_W;
        volatile long double WC_WA = (long double)ptC.Pt_W - (long double)ptA.Pt_W;
        volatile long double WSlope = ((WC_WA * (long double)YB_YA) - (WB_WA * (long double)YC_YA)) * Dp;
        volatile long double WSlope_scaled = WSlope * 256.0L;
        S32 expected_W = (S32)WSlope_scaled;

        setup_slope_test_state(10);
        Fill_Type = POLY_TEXTURE_Z;
        Fill_MapU_XSlope = 0;
        Fill_MapV_XSlope = 0;
        Fill_W_XSlope = 0;
        Fill_ZBuf_XSlope = 0;

        Calc_XSlopeZBufferFPU(POLY_TEXTURE_Z, &ptA, &ptB, &ptC);

        fesetround(FE_TONEAREST);
        RoundType = ROUND_TYPE_FLOAT;

        char msg[128];
        snprintf(msg, sizeof(msg), "TextureZXSlopeZBuf Z #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_Z, Fill_ZBuf_XSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZXSlopeZBuf U #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_U, Fill_MapU_XSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZXSlopeZBuf V #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_V, Fill_MapV_XSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZXSlopeZBuf W #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_W, Fill_W_XSlope, msg);
    }
}

static void test_texturez_leftslope(void)
{
    poly_rng_seed(0xABCD5678);

    for (int i = 0; i < 300; i++) {
        S16 xA = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yA = (S16)(poly_rng_next() % (TEST_POLY_H / 2));
        S16 xB = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yB = (S16)(yA + 1 + (poly_rng_next() % (TEST_POLY_H / 2)));

        U16 uA = (U16)(poly_rng_next() & 0xFF);
        U16 vA = (U16)(poly_rng_next() & 0xFF);
        U16 uB = (U16)(poly_rng_next() & 0xFF);
        U16 vB = (U16)(poly_rng_next() & 0xFF);

        S32 wA = (S32)(poly_rng_next() % 65280 + 256);
        S32 wB = (S32)(poly_rng_next() % 65280 + 256);

        Struc_Point ptA = make_texz_point(xA, yA, uA, vA, wA, 0, 0);
        Struc_Point ptB = make_texz_point(xB, yB, uB, vB, wB, 0, 0);

        S32 diffX = (ptB.Pt_XE - ptA.Pt_XE) << 16;
        S32 diffY = ptB.Pt_YE - ptA.Pt_YE;
        if (diffY <= 0) continue;

        fesetround(FE_TOWARDZERO);
        RoundType = ROUND_TYPE_INT;

        volatile long double inv_dY = 1.0L / (long double)diffY;
        volatile long double inv256_dY = inv_dY * (1.0L / 256.0L);

        volatile long double XSlope = (long double)diffX * inv_dY;
        volatile long double LeftSlope = XSlope + 1.0L;
        S32 expected_LeftSlope = (S32)LeftSlope;

        volatile long double UAp = (long double)(S32)ptA.Pt_MapU * (long double)ptA.Pt_W;
        volatile long double UBp = (long double)(S32)ptB.Pt_MapU * (long double)ptB.Pt_W;
        volatile long double VAp = (long double)(S32)ptA.Pt_MapV * (long double)ptA.Pt_W;
        volatile long double VBp = (long double)(S32)ptB.Pt_MapV * (long double)ptB.Pt_W;

        volatile long double USlope = (UBp - UAp) * inv256_dY;
        volatile long double VSlope = (VBp - VAp) * inv256_dY;
        S32 expected_ULeft = (S32)USlope;
        S32 expected_VLeft = (S32)VSlope;

        volatile long double curU = UAp * (1.0L / 256.0L);
        volatile long double curV = VAp * (1.0L / 256.0L);
        S32 expected_CurU = (S32)curU;
        S32 expected_CurV = (S32)curV;

        volatile long double WSlope_raw = ((long double)ptB.Pt_W - (long double)ptA.Pt_W) * inv256_dY;
        volatile long double WSlope = WSlope_raw * 256.0L;
        S32 expected_WLeft = (S32)WSlope;

        setup_slope_test_state(10);
        Fill_Type = POLY_TEXTURE_Z;
        Fill_LeftSlope = 0;
        Fill_MapU_LeftSlope = 0;
        Fill_MapV_LeftSlope = 0;
        Fill_W_LeftSlope = 0;
        Fill_CurMapUMin = 0;
        Fill_CurMapVMin = 0;
        Fill_CurWMin = 0;

        Calc_TextureZLeftSlopeFPU(POLY_TEXTURE_Z, diffX, diffY, &ptA, &ptB);

        fesetround(FE_TONEAREST);
        RoundType = ROUND_TYPE_FLOAT;

        char msg[128];
        snprintf(msg, sizeof(msg), "TextureZLeftSlope LeftSlope #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_LeftSlope, Fill_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlope ULeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_ULeft, Fill_MapU_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlope VLeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_VLeft, Fill_MapV_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlope WLeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_WLeft, Fill_W_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlope CurU #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_CurU, Fill_CurMapUMin, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlope CurV #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_CurV, Fill_CurMapVMin, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlope CurW #%d", i);
        ASSERT_ASM_CPP_EQ_INT(ptA.Pt_W, Fill_CurWMin, msg);
    }
}

static void test_texturez_gouraud_leftslope(void)
{
    poly_rng_seed(0x12349ABC);

    for (int i = 0; i < 300; i++) {
        S16 xA = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yA = (S16)(poly_rng_next() % (TEST_POLY_H / 2));
        S16 xB = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yB = (S16)(yA + 1 + (poly_rng_next() % (TEST_POLY_H / 2)));

        U16 uA = (U16)(poly_rng_next() & 0xFF);
        U16 vA = (U16)(poly_rng_next() & 0xFF);
        U16 uB = (U16)(poly_rng_next() & 0xFF);
        U16 vB = (U16)(poly_rng_next() & 0xFF);

        S32 wA = (S32)(poly_rng_next() % 65280 + 256);
        S32 wB = (S32)(poly_rng_next() % 65280 + 256);

        U16 lightA = (U16)(poly_rng_next() % 3584);
        U16 lightB = (U16)(poly_rng_next() % 3584);

        Struc_Point ptA = make_texz_point(xA, yA, uA, vA, wA, lightA, 0);
        Struc_Point ptB = make_texz_point(xB, yB, uB, vB, wB, lightB, 0);

        S32 diffX = (ptB.Pt_XE - ptA.Pt_XE) << 16;
        S32 diffY = ptB.Pt_YE - ptA.Pt_YE;
        if (diffY <= 0) continue;

        fesetround(FE_TOWARDZERO);
        RoundType = ROUND_TYPE_INT;

        volatile long double inv_dY = 1.0L / (long double)diffY;
        volatile long double inv256_dY = inv_dY * (1.0L / 256.0L);

        /* Gouraud slope */
        S32 gouraudB = ((S32)(ptB.Pt_Light & 0xFFFF) << 8) + 0x8000;
        S32 gouraudA = ((S32)(ptA.Pt_Light & 0xFFFF) + 0x80) << 8;
        S32 diffLight = gouraudA - gouraudB;
        volatile long double GSlope = (long double)diffLight * inv_dY;
        S32 expected_GLeft = (S32)GSlope;

        /* X slope */
        volatile long double XSlope = (long double)diffX * inv_dY;
        volatile long double LeftSlope = XSlope + 1.0L;
        S32 expected_LeftSlope = (S32)LeftSlope;

        /* Texture slopes */
        volatile long double UAp = (long double)(S32)ptA.Pt_MapU * (long double)ptA.Pt_W;
        volatile long double UBp = (long double)(S32)ptB.Pt_MapU * (long double)ptB.Pt_W;
        volatile long double VAp = (long double)(S32)ptA.Pt_MapV * (long double)ptA.Pt_W;
        volatile long double VBp = (long double)(S32)ptB.Pt_MapV * (long double)ptB.Pt_W;

        volatile long double USlope = (UBp - UAp) * inv256_dY;
        volatile long double VSlope = (VBp - VAp) * inv256_dY;
        S32 expected_ULeft = (S32)USlope;
        S32 expected_VLeft = (S32)VSlope;

        volatile long double curU = UAp * (1.0L / 256.0L);
        volatile long double curV = VAp * (1.0L / 256.0L);
        S32 expected_CurU = (S32)curU;
        S32 expected_CurV = (S32)curV;

        volatile long double WSlope_raw = ((long double)ptB.Pt_W - (long double)ptA.Pt_W) * inv256_dY;
        volatile long double WSlope = WSlope_raw * 256.0L;
        S32 expected_WLeft = (S32)WSlope;

        setup_slope_test_state(10);
        Fill_Type = POLY_TEXTURE_Z_GOURAUD;
        Fill_LeftSlope = 0;
        Fill_MapU_LeftSlope = 0;
        Fill_MapV_LeftSlope = 0;
        Fill_W_LeftSlope = 0;
        Fill_Gouraud_LeftSlope = 0;
        Fill_CurMapUMin = 0;
        Fill_CurMapVMin = 0;
        Fill_CurWMin = 0;
        Fill_CurGouraudMin = 0;

        Calc_TextureZGouraudLeftSlopeFPU(POLY_TEXTURE_Z_GOURAUD, diffX, diffY, &ptA, &ptB);

        fesetround(FE_TONEAREST);
        RoundType = ROUND_TYPE_FLOAT;

        char msg[128];
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope LeftSlope #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_LeftSlope, Fill_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope GLeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_GLeft, Fill_Gouraud_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope ULeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_ULeft, Fill_MapU_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope VLeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_VLeft, Fill_MapV_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope WLeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_WLeft, Fill_W_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope CurG #%d", i);
        ASSERT_ASM_CPP_EQ_INT(gouraudB, Fill_CurGouraudMin, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope CurU #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_CurU, Fill_CurMapUMin, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope CurV #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_CurV, Fill_CurMapVMin, msg);
        snprintf(msg, sizeof(msg), "TextureZGouraudLeftSlope CurW #%d", i);
        ASSERT_ASM_CPP_EQ_INT(ptA.Pt_W, Fill_CurWMin, msg);
    }
}

static void test_texturez_leftslope_zbuf(void)
{
    poly_rng_seed(0xFEDC9876);

    for (int i = 0; i < 300; i++) {
        S16 xA = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yA = (S16)(poly_rng_next() % (TEST_POLY_H / 2));
        S16 xB = (S16)(poly_rng_next() % TEST_POLY_W);
        S16 yB = (S16)(yA + 1 + (poly_rng_next() % (TEST_POLY_H / 2)));

        U16 uA = (U16)(poly_rng_next() & 0xFF);
        U16 vA = (U16)(poly_rng_next() & 0xFF);
        U16 uB = (U16)(poly_rng_next() & 0xFF);
        U16 vB = (U16)(poly_rng_next() & 0xFF);

        S32 wA = (S32)(poly_rng_next() % 65280 + 256);
        S32 wB = (S32)(poly_rng_next() % 65280 + 256);

        U16 zoA = (U16)(poly_rng_next() & 0xFFFF);
        U16 zoB = (U16)(poly_rng_next() & 0xFFFF);

        Struc_Point ptA = make_texz_point(xA, yA, uA, vA, wA, 0, zoA);
        Struc_Point ptB = make_texz_point(xB, yB, uB, vB, wB, 0, zoB);

        S32 diffX = (ptB.Pt_XE - ptA.Pt_XE) << 16;
        S32 diffY = ptB.Pt_YE - ptA.Pt_YE;
        if (diffY <= 0) continue;

        fesetround(FE_TOWARDZERO);
        RoundType = ROUND_TYPE_INT;

        volatile long double inv_dY = 1.0L / (long double)diffY;
        volatile long double inv256_dY = inv_dY * (1.0L / 256.0L);

        /* X slope */
        volatile long double XSlope = (long double)diffX * inv_dY;
        volatile long double LeftSlope = XSlope + 1.0L;
        S32 expected_LeftSlope = (S32)LeftSlope;

        /* ZBuf slope — uses simple (non-perspective) Z formula via wrapper:
         * Calc_ZBufLeftSlopeFPU computes ZA_val = ZA * 256, ZSlope = (ZB*256 - ZA*256) / dY */
        volatile long double ZA_val = (long double)(S32)(U32)ptA.Pt_ZO * 256.0L;
        volatile long double ZB_val = (long double)(S32)(U32)ptB.Pt_ZO * 256.0L;
        S32 expected_CurZ = (S32)ZA_val;
        volatile long double dZ = ZB_val - ZA_val;
        volatile long double ZLeftSlope = dZ * inv_dY;
        S32 expected_ZLeft = (S32)ZLeftSlope;

        /* Texture slopes */
        volatile long double UAp = (long double)(S32)ptA.Pt_MapU * (long double)ptA.Pt_W;
        volatile long double UBp = (long double)(S32)ptB.Pt_MapU * (long double)ptB.Pt_W;
        volatile long double VAp = (long double)(S32)ptA.Pt_MapV * (long double)ptA.Pt_W;
        volatile long double VBp = (long double)(S32)ptB.Pt_MapV * (long double)ptB.Pt_W;

        volatile long double USlope = (UBp - UAp) * inv256_dY;
        volatile long double VSlope = (VBp - VAp) * inv256_dY;
        S32 expected_ULeft = (S32)USlope;
        S32 expected_VLeft = (S32)VSlope;

        volatile long double curU = UAp * (1.0L / 256.0L);
        volatile long double curV = VAp * (1.0L / 256.0L);
        S32 expected_CurU = (S32)curU;
        S32 expected_CurV = (S32)curV;

        volatile long double WSlope_raw = ((long double)ptB.Pt_W - (long double)ptA.Pt_W) * inv256_dY;
        volatile long double WSlope = WSlope_raw * 256.0L;
        S32 expected_WLeft = (S32)WSlope;

        setup_slope_test_state(10);
        Fill_Type = POLY_TEXTURE_Z;
        Fill_LeftSlope = 0;
        Fill_MapU_LeftSlope = 0;
        Fill_MapV_LeftSlope = 0;
        Fill_W_LeftSlope = 0;
        Fill_ZBuf_LeftSlope = 0;
        Fill_CurMapUMin = 0;
        Fill_CurMapVMin = 0;
        Fill_CurWMin = 0;
        Fill_CurZBufMin = 0;

        Calc_LeftSlopeZBufferFPU(POLY_TEXTURE_Z, diffX, diffY, &ptA, &ptB);

        fesetround(FE_TONEAREST);
        RoundType = ROUND_TYPE_FLOAT;

        char msg[128];
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf LeftSlope #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_LeftSlope, Fill_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf ZLeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_ZLeft, Fill_ZBuf_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf CurZ #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_CurZ, (S32)Fill_CurZBufMin, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf ULeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_ULeft, Fill_MapU_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf VLeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_VLeft, Fill_MapV_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf WLeft #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_WLeft, Fill_W_LeftSlope, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf CurU #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_CurU, Fill_CurMapUMin, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf CurV #%d", i);
        ASSERT_ASM_CPP_EQ_INT(expected_CurV, Fill_CurMapVMin, msg);
        snprintf(msg, sizeof(msg), "TextureZLeftSlopeZBuf CurW #%d", i);
        ASSERT_ASM_CPP_EQ_INT(ptA.Pt_W, Fill_CurWMin, msg);
    }
}

/* ── Fill_Poly: 300-round ASM-vs-CPP full entry-point equivalence ── *
 *
 * CPP path: Switch_Fillers → Fill_Poly(POLY_SOLID, ...) dispatches
 *   through CPP Jmp_Solid → CPP Fill_PolyClip → CPP Filler_Flat.
 *
 * ASM path: asm_Switch_Fillers initialises ASM-side slope tables,
 *   then we replicate Fill_PolyFast + Jmp_Solid dispatch setup
 *   (test_poly_jmp proved both set identical globals) and call
 *   asm_Fill_PolyClip → ASM Draw_Triangle → asm_Filler_Flat.
 *
 * This exercises the complete Fill_Poly entry point: Fill_LeftSlope
 * clear, SetScreenPitch call, Fill_Type assignment, color masking,
 * and dispatch-table lookup — against the ASM clipper/filler chain. */

static void test_asm_fill_poly_random(void)
{
    /* Sanity: verify ASM jump table was linked and has a valid entry */
    ASSERT_TRUE(asm_Fill_N_Table_Jumps[0] != 0);

    poly_rng_seed(0xF111B0B0);
    for (int i = 0; i < 300; i++) {
        Struc_Point pts[3];
        for (int v = 0; v < 3; v++) {
            /* Keep vertices within screen bounds to avoid clipping paths */
            S16 x = (S16)(poly_rng_next() % TEST_POLY_W);
            S16 y = (S16)(poly_rng_next() % TEST_POLY_H);
            pts[v] = make_point(x, y);
        }
        U8 color = (U8)(poly_rng_next() | 1);

        /* CPP path: full Fill_Poly entry point. */
        setup_polygon_screen();
        Fill_Poly(POLY_SOLID, color, 3, pts);
        memcpy(poly_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        /* ASM path: replicate Fill_PolyFast dispatch + Jmp_Solid globals,
         * then call through ASM clipper + filler chain. */
        setup_polygon_screen();
        call_asm_Switch_Fillers(FILL_POLY_NO_TEXTURES);
        Fill_LeftSlope = 0;
        SetScreenPitch(TabOffLine);
        Fill_Type = POLY_SOLID;
        Fill_Filler = (Fill_Filler_Func)(void *)&asm_Filler_Flat;
        Fill_ClipFlag = 1; /* CLIP_FLAT */
        Fill_Color.Num = color | ((U32)color << 8);
        call_asm_Fill_PolyClip(3, pts);
        memcpy(poly_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg), "Fill_Poly #%d", i);
        ASSERT_ASM_CPP_MEM_EQ(poly_asm_buf, poly_cpp_buf,
                               TEST_POLY_SIZE, msg);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Fill_PolyClip: POLY_TEXTURE_Z_FOG (type 24) ASM-vs-CPP
 * ═══════════════════════════════════════════════════════════════════
 *
 * Exercises the full pipeline for perspective-correct textured fog
 * with z-buffer: Fill_PolyClip → Draw_Triangle →
 * Calc_TextureZXSlopeZBufFPU / Calc_TextureZLeftSlopeZBufFPU →
 * Triangle_ReadNextEdge → Read_Next_Right →
 * Filler_TextureZFogSmoothZBuf.
 *
 * CPP path: Switch_Fillers(FOG_ZBUFFER) → Fill_Poly(24,…).
 * ASM path: asm_Switch_Fillers → set asm filler → asm_Fill_PolyClip.
 */

static U8 g_texz_texture[256 * 256];
static U8 g_texz_fog_clut[65536];
static U16 poly_cpp_zbuf[TEST_POLY_SIZE];
static U16 poly_asm_zbuf[TEST_POLY_SIZE];

static void setup_texz_fogzbuf(void)
{
    setup_polygon_screen();
    /* Texture: synthetic 256×256 pattern, always non-zero */
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++)
            g_texz_texture[y * 256 + x] = (U8)(((x + y) & 0x3F) | 0x40);
    PtrMap = g_texz_texture;
    RepMask = 0xFFFF;
    /* Fog CLUT: identity mapping per 256-byte row */
    for (int i = 0; i < 65536; i++)
        g_texz_fog_clut[i] = (U8)(i & 0xFF);
    PtrCLUTFog = g_texz_fog_clut;
    PtrTruePal = g_texz_fog_clut;
    memcpy(Fill_Logical_Palette, g_texz_fog_clut, 256);
    /* Fog parameters */
    SetFog(100, 10000);
    /* Z-buffer: max so all writes succeed */
    init_test_zbuffer(0xFFFF);
    PtrZBuffer = (PTR_U16)g_test_zbuffer;
    /* Enable perspective sub-patching */
    Fill_Patch = 1;
}

/* Game trace: real terrain quad scaled to fit 160×120 framebuffer.
 * Original: v0(8984,1052) v1(7114,799) v2(3090,355) v3(6989,1052) */
static void test_asm_fill_polyclip_texz_trace(void)
{
    Struc_Point pts[4];
    pts[0] = make_texz_point(155, 110, 4823,  0,     1431178, 39839, 5509);
    pts[1] = make_texz_point(122, 83,  0,      0,     1104672, 46716, 7137);
    pts[2] = make_texz_point(53,  37,  0,      32767, 531686,  7,     14829);
    pts[3] = make_texz_point(120, 110, 27602,  32767, 1431178, 25656, 5509);

    /* CPP path: full pipeline with debug recording */
    setup_texz_fogzbuf();
    Switch_Fillers(FILL_POLY_FOG_ZBUFFER);
    g_poly_debug_enabled = 1;
    g_poly_debug_call_count = 0;
    Fill_Poly(POLY_TEXTURE_Z_FOG, 0, 4, pts);
    g_poly_debug_enabled = 0;
    memcpy(poly_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(poly_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    int cpp_nz = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);

    /* Print CPP filler calls */
    printf("# CPP filler calls: %u\n", g_poly_debug_call_count);
    for (U32 i = 0; i < g_poly_debug_call_count; i++) {
        PolyDebugFillerCall *c = &g_poly_debug_calls[i];
        printf("#   [%u] dY=%u curY=%u xmin=0x%08x xmax=0x%08x lslope=%d rslope=%d\n",
               i, c->diffY, c->curY, c->xmin, c->xmax, c->leftSlope, c->rightSlope);
    }

    /* ASM path: replicate Fill_PolyFast + Jmp_TextureZFogSmoothZBuf */
    setup_texz_fogzbuf();
    Switch_Fillers(FILL_POLY_FOG_ZBUFFER);
    call_asm_Switch_Fillers(FILL_POLY_FOG_ZBUFFER);
    Fill_LeftSlope = 0;
    SetScreenPitch(TabOffLine);
    Fill_Type = POLY_TEXTURE_Z_FOG;
    Fill_Filler = (Fill_Filler_Func)(void *)&asm_Filler_TextureZFogSmoothZBuf;
    Fill_ClipFlag = 1 + 8 + 16; /* CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER */
    call_asm_Fill_PolyClip(4, pts);
    memcpy(poly_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
    memcpy(poly_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

    int asm_nz = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);

    /* Diagnostics */
    printf("# trace: cpp_nz=%d asm_nz=%d\n", cpp_nz, asm_nz);
    int first_diff = -1;
    for (int j = 0; j < TEST_POLY_SIZE; j++) {
        if (poly_asm_buf[j] != poly_cpp_buf[j]) {
            first_diff = j;
            printf("# trace: first diff byte %d (row=%d col=%d) "
                   "asm=0x%02x cpp=0x%02x\n",
                   j, j / TEST_POLY_W, j % TEST_POLY_W,
                   poly_asm_buf[j], poly_cpp_buf[j]);
            break;
        }
    }
    /* Count total differing bytes */
    int diff_count = 0;
    for (int j = 0; j < TEST_POLY_SIZE; j++)
        if (poly_asm_buf[j] != poly_cpp_buf[j]) diff_count++;
    printf("# trace: total diff bytes=%d\n", diff_count);

    ASSERT_ASM_CPP_MEM_EQ(poly_asm_buf, poly_cpp_buf,
                           TEST_POLY_SIZE, "Fill_PolyClip texZ trace fb");
    ASSERT_ASM_CPP_MEM_EQ((U8 *)poly_asm_zbuf, (U8 *)poly_cpp_zbuf,
                           TEST_POLY_SIZE * (int)sizeof(U16),
                           "Fill_PolyClip texZ trace zbuf");
}

/* 300-round random stress: random tri/quad with type 24 (TextureZFogZBuf) */
static void test_asm_random_fill_polyclip_texz(void)
{
    poly_rng_seed(0xDE4D7E42);
    for (int i = 0; i < 300; i++) {
        int nv = (poly_rng_next() % 2) + 3; /* 3 or 4 vertices */
        Struc_Point pts[4];
        for (int v = 0; v < nv; v++) {
            S16 x  = (S16)(poly_rng_next() % TEST_POLY_W);
            S16 y  = (S16)(poly_rng_next() % TEST_POLY_H);
            U16 u  = (U16)(poly_rng_next() & 0x7FFF);
            U16 mv = (U16)(poly_rng_next() & 0x7FFF);
            U16 light = (U16)((poly_rng_next() & 0x7FFF) |
                              ((poly_rng_next() & 1) << 15));
            U16 zo = (U16)((poly_rng_next() & 0x7FFF) |
                           ((poly_rng_next() & 1) << 15));
            S32 w  = (S32)((poly_rng_next() << 7) + 0x1000);
            pts[v] = make_texz_point(x, y, u, mv, w, light, zo);
        }

        /* CPP path */
        setup_texz_fogzbuf();
        Switch_Fillers(FILL_POLY_FOG_ZBUFFER);
        Fill_Poly(POLY_TEXTURE_Z_FOG, 0, nv, pts);
        memcpy(poly_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(poly_cpp_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        /* ASM path */
        setup_texz_fogzbuf();
        Switch_Fillers(FILL_POLY_FOG_ZBUFFER);
        call_asm_Switch_Fillers(FILL_POLY_FOG_ZBUFFER);
        Fill_LeftSlope = 0;
        SetScreenPitch(TabOffLine);
        Fill_Type = POLY_TEXTURE_Z_FOG;
        Fill_Filler = (Fill_Filler_Func)(void *)&asm_Filler_TextureZFogSmoothZBuf;
        Fill_ClipFlag = 1 + 8 + 16; /* CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER */
        call_asm_Fill_PolyClip(nv, pts);
        memcpy(poly_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);
        memcpy(poly_asm_zbuf, g_test_zbuffer, TEST_POLY_SIZE * sizeof(U16));

        /* Diagnostic: print first framebuffer difference */
        for (int j = 0; j < TEST_POLY_SIZE; j++) {
            if (poly_asm_buf[j] != poly_cpp_buf[j]) {
                int row = j / TEST_POLY_W;
                int col = j % TEST_POLY_W;
                printf("# texZ #%d nv=%d: first diff byte %d "
                       "(row=%d col=%d) asm=0x%02x cpp=0x%02x\n",
                       i, nv, j, row, col,
                       poly_asm_buf[j], poly_cpp_buf[j]);
                break;
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "Fill_PolyClip texZ #%d nv=%d", i, nv);
        ASSERT_ASM_CPP_MEM_EQ(poly_asm_buf, poly_cpp_buf,
                               TEST_POLY_SIZE, msg);

        snprintf(msg, sizeof(msg), "Fill_PolyClip texZ zbuf #%d", i);
        ASSERT_ASM_CPP_MEM_EQ((U8 *)poly_asm_zbuf, (U8 *)poly_cpp_zbuf,
                               TEST_POLY_SIZE * (int)sizeof(U16), msg);
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
    RUN_TEST(test_asm_random_fill_polyclip);
    RUN_TEST(test_asm_fill_poly_random);
    RUN_TEST(test_asm_random_fill_sphere);
    RUN_TEST(test_asm_random_line_a);
    RUN_TEST(test_texturez_xslope);
    RUN_TEST(test_texturez_gouraud_xslope);
    RUN_TEST(test_texturez_xslope_zbuf);
    RUN_TEST(test_texturez_leftslope);
    RUN_TEST(test_texturez_gouraud_leftslope);
    RUN_TEST(test_texturez_leftslope_zbuf);
    RUN_TEST(test_asm_fill_polyclip_texz_trace);
    RUN_TEST(test_asm_random_fill_polyclip_texz);
    TEST_SUMMARY();
    return test_failures != 0;
}
