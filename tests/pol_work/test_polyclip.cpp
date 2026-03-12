/* Test: ClipperZ — Z-plane polygon clipping.
 *
 * ClipperZ clips a polygon against a Z-plane, producing a new vertex
 * list.  Uses C-adapted calling convention in both ASM and CPP.
 */
#include "test_harness.h"
#include <POLYGON/CLIPPERZ.H>
#include <string.h>

extern "C" U32 asm_ClipperZ(STRUC_CLIPVERTEX dst[], STRUC_CLIPVERTEX src[],
                             U32 nbvertex, S32 zclip, S32 flag);

static STRUC_CLIPVERTEX src[8], dst_cpp[16], dst_asm[16];

static STRUC_CLIPVERTEX make_cv(S32 x, S32 y, S32 z)
{
    STRUC_CLIPVERTEX v;
    memset(&v, 0, sizeof(v));
    v.V_X0 = x; v.V_Y0 = y; v.V_Z0 = z;
    return v;
}

/* ── CPP-only sanity checks ────────────────────────────────────── */

static void test_all_in_front(void)
{
    /* Triangle entirely in front of z=100, flag=0 (positive half-space) */
    src[0] = make_cv(0, 0, 200);
    src[1] = make_cv(100, 0, 300);
    src[2] = make_cv(50, 80, 400);
    memset(dst_cpp, 0, sizeof(dst_cpp));
    U32 n = ClipperZ(dst_cpp, src, 3, 100, 0);
    /* All vertices in front — output == input, 3 vertices */
    ASSERT_EQ_UINT(3, n);
}

static void test_all_behind(void)
{
    /* Triangle entirely behind z=500, flag=0 */
    src[0] = make_cv(0, 0, 200);
    src[1] = make_cv(100, 0, 300);
    src[2] = make_cv(50, 80, 400);
    memset(dst_cpp, 0, sizeof(dst_cpp));
    U32 n = ClipperZ(dst_cpp, src, 3, 500, 0);
    /* All behind — should get 0 or (U32)-1 (fully clipped) */
    ASSERT_TRUE(n == 0 || n == (U32)-1);
}

static void test_one_behind(void)
{
    /* Two verts at z=200,300 (front), one at z=50 (behind z=100) */
    src[0] = make_cv(0, 0, 50);
    src[1] = make_cv(100, 0, 200);
    src[2] = make_cv(50, 80, 300);
    memset(dst_cpp, 0, sizeof(dst_cpp));
    U32 n = ClipperZ(dst_cpp, src, 3, 100, 0);
    /* Clipping adds 2 new verts, removes 1 → 4 output vertices */
    ASSERT_EQ_UINT(4, n);
}

/* ── ASM-vs-CPP equivalence ────────────────────────────────────── */

static void test_asm_equiv_all_in_front(void)
{
    src[0] = make_cv(0, 0, 200);
    src[1] = make_cv(100, 0, 300);
    src[2] = make_cv(50, 80, 400);
    memset(dst_cpp, 0, sizeof(dst_cpp));
    memset(dst_asm, 0, sizeof(dst_asm));

    U32 n_cpp = ClipperZ(dst_cpp, src, 3, 100, 0);
    U32 n_asm = asm_ClipperZ(dst_asm, src, 3, 100, 0);

    ASSERT_ASM_CPP_EQ_INT((S32)n_asm, (S32)n_cpp, "ClipperZ all-in-front count");
    if (n_cpp == n_asm && n_cpp > 0 && n_cpp < 16) {
        ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp,
                              n_cpp * sizeof(STRUC_CLIPVERTEX),
                              "ClipperZ all-in-front data");
    }
}

static void test_asm_equiv_one_behind(void)
{
    src[0] = make_cv(0, 0, 50);
    src[1] = make_cv(100, 0, 200);
    src[2] = make_cv(50, 80, 300);
    memset(dst_cpp, 0, sizeof(dst_cpp));
    memset(dst_asm, 0, sizeof(dst_asm));

    U32 n_cpp = ClipperZ(dst_cpp, src, 3, 100, 0);
    U32 n_asm = asm_ClipperZ(dst_asm, src, 3, 100, 0);

    ASSERT_ASM_CPP_EQ_INT((S32)n_asm, (S32)n_cpp, "ClipperZ one-behind count");
    if (n_cpp == n_asm && n_cpp > 0 && n_cpp < 16) {
        ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp,
                              n_cpp * sizeof(STRUC_CLIPVERTEX),
                              "ClipperZ one-behind data");
    }
}

static void test_asm_equiv_all_behind(void)
{
    src[0] = make_cv(0, 0, 200);
    src[1] = make_cv(100, 0, 300);
    src[2] = make_cv(50, 80, 400);
    memset(dst_cpp, 0, sizeof(dst_cpp));
    memset(dst_asm, 0, sizeof(dst_asm));

    U32 n_cpp = ClipperZ(dst_cpp, src, 3, 500, 0);
    U32 n_asm = asm_ClipperZ(dst_asm, src, 3, 500, 0);

    ASSERT_ASM_CPP_EQ_INT((S32)n_asm, (S32)n_cpp, "ClipperZ all-behind count");
}

static void test_asm_equiv_negative_flag(void)
{
    /* flag=1 means clip against negative half-space */
    src[0] = make_cv(0, 0, 50);
    src[1] = make_cv(100, 0, 200);
    src[2] = make_cv(50, 80, 300);
    memset(dst_cpp, 0, sizeof(dst_cpp));
    memset(dst_asm, 0, sizeof(dst_asm));

    U32 n_cpp = ClipperZ(dst_cpp, src, 3, 100, 1);
    U32 n_asm = asm_ClipperZ(dst_asm, src, 3, 100, 1);

    ASSERT_ASM_CPP_EQ_INT((S32)n_asm, (S32)n_cpp, "ClipperZ neg-flag count");
    if (n_cpp == n_asm && n_cpp > 0 && n_cpp < 16) {
        ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp,
                              n_cpp * sizeof(STRUC_CLIPVERTEX),
                              "ClipperZ neg-flag data");
    }
}

/* ── Randomized stress test ────────────────────────────────────── */

static U32 rng_state;
static void rng_seed(U32 s) { rng_state = s; }
static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

static void test_asm_equiv_random(void)
{
    rng_seed(0xDEADBEEF);
    for (int i = 0; i < 30; i++) {
        int nverts = 3 + (int)(rng_next() % 3);  /* 3-5 vertices */
        S32 zclip = (S32)(rng_next() % 500) + 50;
        S32 flag = (S32)(rng_next() & 1);
        for (int v = 0; v < nverts; v++) {
            src[v] = make_cv(
                (S32)(rng_next() % 640) - 320,
                (S32)(rng_next() % 480) - 240,
                (S32)(rng_next() % 1000)
            );
        }
        memset(dst_cpp, 0, sizeof(dst_cpp));
        memset(dst_asm, 0, sizeof(dst_asm));

        U32 n_cpp = ClipperZ(dst_cpp, src, nverts, zclip, flag);
        U32 n_asm = asm_ClipperZ(dst_asm, src, nverts, zclip, flag);

        ASSERT_ASM_CPP_EQ_INT((S32)n_asm, (S32)n_cpp, "ClipperZ random count");
        if (n_cpp == n_asm && n_cpp > 0 && n_cpp < 16) {
            ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp,
                                  n_cpp * sizeof(STRUC_CLIPVERTEX),
                                  "ClipperZ random data");
        }
    }
}

int main(void)
{
    RUN_TEST(test_all_in_front);
    RUN_TEST(test_all_behind);
    RUN_TEST(test_one_behind);
    RUN_TEST(test_asm_equiv_all_in_front);
    RUN_TEST(test_asm_equiv_one_behind);
    RUN_TEST(test_asm_equiv_all_behind);
    RUN_TEST(test_asm_equiv_negative_flag);
    RUN_TEST(test_asm_equiv_random);
    TEST_SUMMARY();
    return test_failures != 0;
}
