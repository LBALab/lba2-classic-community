/* Test: ASM-vs-CPP equivalence for all Jmp_* dispatch functions (POLY_JMP).
 *
 * Each Jmp_* function sets globals (Fill_Filler, Fill_ClipFlag, Fill_Color,
 * Fill_Trame_Parity, IsPolygonHidden) and tail-calls Fill_PolyClip.
 * We override Fill_PolyClip with a no-op stub, call each entry from
 * both the CPP and ASM tables, and compare the globals byte-for-byte.
 *
 * Linked with -Wl,--allow-multiple-definition so our Fill_PolyClip stub
 * takes precedence over the library definition.
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <POLYGON/POLY_JMP.H>
#include "poly_test_fixture.h"
#include <string.h>
#include <stdio.h>

/* ── Stub Fill_PolyClip ──────────────────────────────────────────── */
/* The Jmp_* functions set globals then call Fill_PolyClip.  We only
 * care about the globals, so the stub just returns 0.               */
extern "C" S32 Fill_PolyClip(S32 Nb_Points, Struc_Point *Ptr_Points)
{
    (void)Nb_Points;
    (void)Ptr_Points;
    return 0;
}

/* ── ASM jump tables (renamed to asm_ prefix by objcopy) ─────────── */
extern "C" {
    extern U32 asm_Fill_N_Table_Jumps[];
    extern U32 asm_Fill_Fog_Table_Jumps[];
    extern U32 asm_Fill_ZBuf_Table_Jumps[];
    extern U32 asm_Fill_FogZBuf_Table_Jumps[];
    extern U32 asm_Fill_NZW_Table_Jumps[];
    extern U32 asm_Fill_FogNZW_Table_Jumps[];
}

/* ── Inline ASM wrapper for Watcom register-convention calls ─────── */
/* ASM Jmp_* convention: ebx = color, ecx = Nb_Points, esi = Ptr_Points.
 * Bare PROCs (no params / no uses) have no UASM-generated prologue.
 * They tail-call Fill_PolyClip via jmp; our stub returns normally.   */
static S32 call_asm_jmp(U32 fn_addr, S32 nb_points,
                        Struc_Point *ptr_points, U16 color)
{
    S32 result;
    void *fn = (void *)(uintptr_t)fn_addr;
    U32 color32 = (U32)color;
    S32 nb = nb_points;
    Struc_Point *pts = ptr_points;
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call *%%edi\n\t"
        "pop  %%ebp"
        : "=a"(result), "+D"(fn), "+b"(color32), "+c"(nb), "+S"(pts)
        :
        : "edx", "memory", "cc"
    );
    return result;
}

#define NUM_ENTRIES    26
#define RANDOM_ROUNDS  50

/* ── State snapshot for comparison ───────────────────────────────── */
typedef struct {
    Fill_Filler_Func filler;
    U8   clip_flag;
    U32  color_num;
    S32  trame_parity;
    U32  is_hidden;
} JmpState;

static void reset_state(void)
{
    Fill_Filler       = NULL;
    Fill_ClipFlag     = 0xFF;
    Fill_Color.Num    = 0xDEADBEEF;
    Fill_Trame_Parity = -1;
    IsPolygonHidden   = 0xBAADF00D;
}

static JmpState snap(void)
{
    JmpState s;
    s.filler       = Fill_Filler;
    s.clip_flag    = Fill_ClipFlag;
    s.color_num    = Fill_Color.Num;
    s.trame_parity = Fill_Trame_Parity;
    s.is_hidden    = IsPolygonHidden;
    return s;
}

/* ── Prerequisites (palette + CLUT for fog / gouraud entries) ───── */
static U8 g_clut[256 * 256];

static void setup_prereqs(void)
{
    for (int i = 0; i < 256 * 256; i++)
        g_clut[i] = (U8)(i & 0xFF);
    PtrCLUTGouraud = g_clut;
    for (int i = 0; i < 256; i++)
        Fill_Logical_Palette[i] = (U8)(255 - i);
}

/* ── Compare one table entry (ASM vs CPP) ────────────────────────── */
static void compare_entry(Fill_Jump_Fn cpp_fn, U32 asm_addr,
                          S32 nb, Struc_Point *pts, U16 color,
                          const char *label)
{
    char msg[256];

    /* CPP path */
    reset_state();
    cpp_fn(nb, pts, color);
    JmpState cs = snap();

    /* ASM path */
    reset_state();
    call_asm_jmp(asm_addr, nb, pts, color);
    JmpState as = snap();

    snprintf(msg, sizeof(msg), "%s Fill_Filler", label);
    ASSERT_ASM_CPP_EQ_INT((long long)(uintptr_t)as.filler,
                          (long long)(uintptr_t)cs.filler, msg);

    snprintf(msg, sizeof(msg), "%s Fill_ClipFlag", label);
    ASSERT_ASM_CPP_EQ_INT((int)as.clip_flag, (int)cs.clip_flag, msg);

    snprintf(msg, sizeof(msg), "%s Fill_Color", label);
    ASSERT_ASM_CPP_EQ_INT((long long)as.color_num,
                          (long long)cs.color_num, msg);

    snprintf(msg, sizeof(msg), "%s Fill_Trame_Parity", label);
    ASSERT_ASM_CPP_EQ_INT(as.trame_parity, cs.trame_parity, msg);

    snprintf(msg, sizeof(msg), "%s IsPolygonHidden", label);
    ASSERT_ASM_CPP_EQ_INT((long long)as.is_hidden,
                          (long long)cs.is_hidden, msg);
}

/* ── Test helper: iterate all entries of one table ───────────────── */
static void test_table(Fill_Jump_Fn *cpp_tbl, U32 *asm_tbl,
                       const char *name, int rounds)
{
    Struc_Point pts[3];
    memset(pts, 0, sizeof(pts));
    pts[0] = make_point(80, 10);
    pts[1] = make_point(40, 100);
    pts[2] = make_point(120, 100);
    setup_prereqs();

    for (int idx = 0; idx < NUM_ENTRIES; idx++) {
        if (!cpp_tbl[idx] || !asm_tbl[idx]) continue;

        char label[128];

        /* Static test with fixed color */
        snprintf(label, sizeof(label), "%s[%d] color=0x42", name, idx);
        compare_entry(cpp_tbl[idx], asm_tbl[idx], 3, pts, 0x42, label);

        /* Randomized rounds */
        poly_rng_seed(0xDEADBEEF + (U32)idx);
        for (int r = 0; r < rounds; r++) {
            U16 color = (U16)(poly_rng_next() & 0xFF);
            snprintf(label, sizeof(label), "%s[%d] rnd#%d c=0x%02X",
                     name, idx, r, color);
            compare_entry(cpp_tbl[idx], asm_tbl[idx], 3, pts, color, label);
        }
    }
}

/* ── One RUN_TEST per table ──────────────────────────────────────── */

static void test_N_table(void)
{
    test_table(Fill_N_Table_Jumps, asm_Fill_N_Table_Jumps,
               "N", RANDOM_ROUNDS);
}

static void test_Fog_table(void)
{
    test_table(Fill_Fog_Table_Jumps, asm_Fill_Fog_Table_Jumps,
               "Fog", RANDOM_ROUNDS);
}

static void test_ZBuf_table(void)
{
    test_table(Fill_ZBuf_Table_Jumps, asm_Fill_ZBuf_Table_Jumps,
               "ZBuf", RANDOM_ROUNDS);
}

static void test_FogZBuf_table(void)
{
    test_table(Fill_FogZBuf_Table_Jumps, asm_Fill_FogZBuf_Table_Jumps,
               "FogZBuf", RANDOM_ROUNDS);
}

static void test_NZW_table(void)
{
    test_table(Fill_NZW_Table_Jumps, asm_Fill_NZW_Table_Jumps,
               "NZW", RANDOM_ROUNDS);
}

static void test_FogNZW_table(void)
{
    test_table(Fill_FogNZW_Table_Jumps, asm_Fill_FogNZW_Table_Jumps,
               "FogNZW", RANDOM_ROUNDS);
}

int main(void)
{
    RUN_TEST(test_N_table);
    RUN_TEST(test_Fog_table);
    RUN_TEST(test_ZBuf_table);
    RUN_TEST(test_FogZBuf_table);
    RUN_TEST(test_NZW_table);
    RUN_TEST(test_FogNZW_table);
    TEST_SUMMARY();
    return test_failures != 0;
}
