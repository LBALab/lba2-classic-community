/* The arithmetic a recording is replayed against: LIB386/3D's long double paths.
 *
 * A `.rec` header declares `numeric.long_double_bits` so that a replay on a host
 * whose arithmetic differs opens by saying so (docs/RECORDING.md). Until this
 * file existed, nothing measured the thing that field describes. The tests that
 * own these functions, tests/3D, are ASM-versus-C equivalence, need the 32-bit
 * UASM toolchain, and run only in the Docker leg, so they never execute on macOS
 * or Windows. tests/precision_paths runs everywhere but deliberately asserts
 * nothing about the values, on the grounds that it had "no standing to define a
 * precision contract".
 *
 * There is standing now: the recording format is a caller that has to know when
 * this arithmetic changes.
 *
 * What it does depends on the host, and deliberately so:
 *
 *   LDBL_MANT_DIG == 64 (x86-64, where every committed baseline was made)
 *       Asserts the committed vector. This is the regression net.
 *
 *   Anything else (ARM: macOS, Android, where long double is a plain double)
 *       Reports each value against the vector and the largest difference, and
 *       asserts only the properties that hold on any host. It does not assert
 *       the vector, because inventing a precision contract for a platform nobody
 *       has measured is how a test comes to encode a guess.
 *
 * The second case is the point of running this on macOS at all. CI runs
 * `ctest -L host_quick` there on every PR, so the job log carries the ARM
 * numbers, and whoever picks up ARM replay reads the size of the problem off a
 * CI log instead of needing the hardware. When those numbers are known and
 * wanted, the reporting branch is where the assertion goes.
 *
 * Regenerate the vector (only when the arithmetic is meant to change, and only
 * on an x86-64 host):
 *   LBA2_NUMERIC_EMIT=1 ./build/tests/numeric_contract/test_numeric_contract
 */

#include "test_harness.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#include <3D/CAMERA.H>
#include <3D/DISTANCE.H>
#include <3D/LIROT3D.H>
#include <3D/LPROJ.H>
#include <3D/PROJREC.H>
#include <3D/ROT2D.H>
#include <3D/ROT3D.H>

/* Owned by CAMERA.CPP in the engine, which would drag the whole matrix and
   projection stack into a test that needs a dozen scalars. tests/precision_paths
   does the same for the three it needs. */
S32 X0 = 0;
S32 Y0 = 0;
S32 Z0 = 0;
S32 Xp = 0;
S32 Yp = 0;
S32 CameraXr = 0;
S32 CameraYr = 0;
S32 CameraZr = 0;
S32 CameraZrClip = 0;
S32 XCentre = 0;
S32 YCentre = 0;
float FRatioX = 0.0f;
float FRatioY = 0.0f;

/* The projection recorder is a capture hook, not arithmetic. Stubbed so the test
   links the maths and nothing else. */
void Projrec_LongProjectPoint3D(S32 x, S32 y, S32 z, S32 ret, S32 xp, S32 yp) {
    (void)x;
    (void)y;
    (void)z;
    (void)ret;
    (void)xp;
    (void)yp;
}

/* 64 on x86-64 with x87 extended precision, 53 where long double is a double. */
#define HAS_X87_LONG_DOUBLE (LDBL_MANT_DIG == 64)

/* ── the committed vector ──────────────────────────────────────────────────
 *
 * Every value below was produced on x86-64 and cross-checked on a second
 * x86-64 host with a different toolchain (Linux GCC and Windows MSYS2 UCRT64).
 * Regenerate with LBA2_NUMERIC_EMIT=1, and only when the change to the
 * arithmetic is deliberate: these numbers are what a recording made on one host
 * is replayed against on another. */
static const S32 EXPECTED[] = {
#include "numeric_contract_vector.inc"
};

#define NEXPECTED ((int)(sizeof EXPECTED / sizeof EXPECTED[0]))

static int g_emit = 0;
static int g_idx = 0;
static long g_maxDelta = 0;
static const char *g_worstLabel = "none";
static int g_reported = 0;

/* One measured value. On an x87 host this is an assertion; elsewhere it is a
   measurement, printed and accumulated into the largest difference. */
static void check(const char *label, S32 got) {
    if (g_emit) {
        printf("    %d, /* %s */\n", (int)got, label);
        g_idx++;
        return;
    }
    if (g_idx >= NEXPECTED) {
        fprintf(stderr, "  more values than the vector holds at '%s'\n", label);
        ASSERT_TRUE(g_idx < NEXPECTED);
        return;
    }
    {
        S32 want = EXPECTED[g_idx++];
#if HAS_X87_LONG_DOUBLE
        if (want != got)
            fprintf(stderr, "  %s: expected %d, got %d\n", label, (int)want, (int)got);
        ASSERT_EQ_INT(want, got);
#else
        long delta = (long)got - (long)want;
        if (delta < 0)
            delta = -delta;
        if (delta > g_maxDelta) {
            g_maxDelta = delta;
            g_worstLabel = label;
        }
        if (delta != 0 && g_reported < 12) {
            printf("#   %-34s x86-64 %-12d here %-12d delta %ld\n", label, (int)want,
                   (int)got, delta);
            g_reported++;
        }
#endif
    }
}

/* ── the cases ─────────────────────────────────────────────────────────────
 *
 * Chosen for where the arithmetic is most likely to part company: values whose
 * products need more than 53 bits of mantissa before the rounding, and inputs
 * that land on or beside a rounding boundary. */

/* Distance is the one on this list that the simulation reads directly: actor
   separation drives behaviour, so a difference here is a different game, not a
   different picture.
 *
 * Every case below stays inside the defined domain, and the domain is smaller
 * than it looks. DISTANCE.CPP squares the deltas in `int` before promoting the
 * sum to long double, so `dx * dx` overflows a signed 32-bit integer once the
 * separation passes 46340 in 2D, or 26754 on each axis in 3D. That is undefined
 * behaviour, which means it is not a value to pin: it is not stable across
 * compilers or optimisation levels, so a vector containing it would be a flaky
 * test rather than a contract. The last case of each set sits on the boundary,
 * which is where a future widening of that arithmetic would first show up.
 *
 * The overflow itself is a live defect and is not this test's to fix. */
static void case_distance(void) {
    static const S32 d2[][4] = {
        {0, 0, 0, 0},
        {0, 0, 3, 4},
        {0, 0, 1, 1},
        {-5000, 2500, 7000, -1200},
        {7, 11, 13, 17},
        {-30000, 10000, 2000, -15000},
        {0, 0, 46340, 0}, /* the largest 2D separation that does not overflow */
    };
    static const S32 d3[][6] = {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 3, 4, 12},
        {0, 0, 0, 1, 1, 1},
        {-5000, 2500, 900, 7000, -1200, -400},
        {20000, -15000, 5000, -1000, 2000, -3000},
        {26754, 26754, 26754, 0, 0, 0}, /* on the 3D boundary */
    };
    char label[64];
    int i;

    for (i = 0; i < (int)(sizeof d2 / sizeof d2[0]); i++) {
        snprintf(label, sizeof label, "Distance2D[%d]", i);
        check(label, (S32)Distance2D(d2[i][0], d2[i][1], d2[i][2], d2[i][3]));
    }
    for (i = 0; i < (int)(sizeof d3 / sizeof d3[0]); i++) {
        snprintf(label, sizeof label, "Distance3D[%d]", i);
        check(label,
              (S32)Distance3D(d3[i][0], d3[i][1], d3[i][2], d3[i][3], d3[i][4], d3[i][5]));
    }
}

/* A matrix whose entries are not all exactly representable, so the products
   carry mantissa bits past what a double would keep. */
static void fill_matrix(TYPE_MAT *m) {
    m->F.M11 = 0.8137334704f;
    m->F.M12 = -0.5811947584f;
    m->F.M13 = 0.0000000000f;
    m->F.M21 = 0.5811947584f;
    m->F.M22 = 0.8137334704f;
    m->F.M23 = 0.0000000000f;
    m->F.M31 = 0.0000000000f;
    m->F.M32 = 0.0000000000f;
    m->F.M33 = 1.0000000000f;
    m->F.TX = 0.0f;
    m->F.TY = 0.0f;
    m->F.TZ = 0.0f;
}

static const S32 kCoords[] = {0, 1, -1, 511, -512, 32767, -32768, 1000000, -1000000, 16777217};

static void case_rotate3d(void) {
    TYPE_MAT m;
    char label[64];
    int i;

    fill_matrix(&m);
    for (i = 0; i < (int)(sizeof kCoords / sizeof kCoords[0]); i++) {
        LongRotatePoint(&m, kCoords[i], kCoords[(i + 3) % 10], kCoords[(i + 7) % 10]);
        snprintf(label, sizeof label, "LongRotatePointF[%d].X", i);
        check(label, X0);
        snprintf(label, sizeof label, "LongRotatePointF[%d].Y", i);
        check(label, Y0);
        snprintf(label, sizeof label, "LongRotatePointF[%d].Z", i);
        check(label, Z0);
    }
}

static void case_inverse_rotate3d(void) {
    TYPE_MAT m;
    char label[64];
    int i;

    fill_matrix(&m);
    for (i = 0; i < (int)(sizeof kCoords / sizeof kCoords[0]); i++) {
        LongInverseRotatePoint(&m, kCoords[i], kCoords[(i + 3) % 10], kCoords[(i + 7) % 10]);
        snprintf(label, sizeof label, "LongInverseRotatePointF[%d].X", i);
        check(label, X0);
        snprintf(label, sizeof label, "LongInverseRotatePointF[%d].Y", i);
        check(label, Y0);
        snprintf(label, sizeof label, "LongInverseRotatePointF[%d].Z", i);
        check(label, Z0);
    }
}

/* The subject of #525: same control flow on two x86-64 hosts, different hashes
   of what was projected. If this case agrees on both, the divergence is not in
   the projection arithmetic and the search moves to what feeds it. */
static void case_project3d(void) {
    static const S32 pts[][3] = {
        {0, 0, 0},
        {1, 1, -1},
        {511, -512, -1000},
        {-4096, 2048, -8192},
        {32767, -32768, -1},
        {1000000, -1000000, -2000000},
        {123, 456, -789},
        {-1, -1, -1},
    };
    char label[64];
    int i;

    CameraXr = 517;
    CameraYr = -3719;
    CameraZr = 12345;
    CameraZrClip = 100000; /* above every z below, so the clip branch is not taken */
    XCentre = 320;
    YCentre = 240;
    FRatioX = 977.0f;
    FRatioY = 0.8f;

    for (i = 0; i < (int)(sizeof pts / sizeof pts[0]); i++) {
        S32 ret = LongProjectPoint3D(pts[i][0], pts[i][1], pts[i][2]);
        snprintf(label, sizeof label, "LongProjectPoint3D[%d].ret", i);
        check(label, ret);
        snprintf(label, sizeof label, "LongProjectPoint3D[%d].Xp", i);
        check(label, Xp);
        snprintf(label, sizeof label, "LongProjectPoint3D[%d].Yp", i);
        check(label, Yp);
    }
}

static void case_rotate2d(void) {
    static const S32 angles[] = {1, 341, 1023, 1024, 1025, 2047, 3072, 4095};
    char label[64];
    int a, i;

    for (a = 0; a < (int)(sizeof angles / sizeof angles[0]); a++) {
        for (i = 0; i < 4; i++) {
            LongRotate(kCoords[i + 3], kCoords[i + 5], angles[a]);
            snprintf(label, sizeof label, "LongRotate[%d][%d].X", a, i);
            check(label, X0);
            snprintf(label, sizeof label, "LongRotate[%d][%d].Z", a, i);
            check(label, Z0);
        }
    }
}

static void test_vector(void) {
    case_distance();
    case_rotate3d();
    case_inverse_rotate3d();
    case_project3d();
    case_rotate2d();

    if (g_emit)
        return;

    /* A vector longer than the cases means a case was dropped without the vector
       being regenerated, which would silently stop checking the tail. */
    ASSERT_EQ_INT(NEXPECTED, g_idx);

#if !HAS_X87_LONG_DOUBLE
    printf("# long double is %d mantissa bits here, not 64: the vector above is\n",
           (int)LDBL_MANT_DIG);
    printf("# reported, not asserted. Largest difference %ld, at %s.\n", g_maxDelta,
           g_worstLabel);
    printf("# A recording made on x86-64 does not replay here until that is 0.\n");
#endif
}

/* ── properties that hold on any host ──────────────────────────────────────
 *
 * These are what keeps the test honest where the vector is not asserted: on ARM
 * the run above measures, and these still fail if the arithmetic is broken
 * rather than merely different. */

static void test_distance_properties(void) {
    int i;
    /* A point is no distance from itself, and distance does not care which way
       round its arguments are. */
    for (i = 0; i < (int)(sizeof kCoords / sizeof kCoords[0]); i++) {
        S32 x = kCoords[i], y = kCoords[(i + 3) % 10], z = kCoords[(i + 7) % 10];
        ASSERT_EQ_UINT(0u, Distance3D(x, y, z, x, y, z));
        ASSERT_EQ_UINT(0u, Distance2D(x, y, x, y));
        ASSERT_EQ_UINT(Distance3D(x, y, z, 0, 0, 0), Distance3D(0, 0, 0, x, y, z));
        ASSERT_EQ_UINT(Distance2D(x, y, 0, 0), Distance2D(0, 0, x, y));
    }
    /* Exact triples, so the result is an integer on any arithmetic. */
    ASSERT_EQ_UINT(5u, Distance2D(0, 0, 3, 4));
    ASSERT_EQ_UINT(13u, Distance3D(0, 0, 0, 3, 4, 12));

    /* The edge of the defined domain, named so that the next person to widen a
       distance case knows where the cliff is rather than discovering it as a
       vector that will not reproduce. Both of these are the last input that does
       not overflow the `int` squaring inside DISTANCE.CPP. */
    ASSERT_EQ_UINT(46340u, Distance2D(0, 0, 46340, 0));
    ASSERT_EQ_UINT(46339u, Distance3D(0, 0, 0, 26754, 26754, 26754));
}

static void test_rotation_properties(void) {
    TYPE_MAT id;
    int i;

    id.F.M11 = 1.0f;
    id.F.M12 = 0.0f;
    id.F.M13 = 0.0f;
    id.F.M21 = 0.0f;
    id.F.M22 = 1.0f;
    id.F.M23 = 0.0f;
    id.F.M31 = 0.0f;
    id.F.M32 = 0.0f;
    id.F.M33 = 1.0f;
    id.F.TX = id.F.TY = id.F.TZ = 0.0f;

    for (i = 0; i < (int)(sizeof kCoords / sizeof kCoords[0]); i++) {
        S32 x = kCoords[i], y = kCoords[(i + 3) % 10], z = kCoords[(i + 7) % 10];

        LongRotatePoint(&id, x, y, z);
        ASSERT_EQ_INT(x, X0);
        ASSERT_EQ_INT(y, Y0);
        ASSERT_EQ_INT(z, Z0);

        /* The inverse rotation is the transpose, and the identity is its own. */
        LongInverseRotatePoint(&id, x, y, z);
        ASSERT_EQ_INT(x, X0);
        ASSERT_EQ_INT(y, Y0);
        ASSERT_EQ_INT(z, Z0);
    }
}

static void test_projection_properties(void) {
    CameraXr = 517;
    CameraYr = -3719;
    CameraZr = 12345;
    CameraZrClip = 100000;
    XCentre = 320;
    YCentre = 240;
    FRatioX = 977.0f;
    FRatioY = 0.8f;

    /* The camera's own position projects to the centre of the screen, whatever
       the arithmetic: both offsets are zero before the multiply. */
    ASSERT_EQ_INT(1, LongProjectPoint3D(CameraXr, CameraYr, 0));
    ASSERT_EQ_INT(XCentre, Xp);
    ASSERT_EQ_INT(YCentre, Yp);

    /* Past the clip plane the function reports failure and pins both outputs,
       which is a branch rather than a rounding, so it holds everywhere. */
    CameraZrClip = 0;
    ASSERT_EQ_INT(0, LongProjectPoint3D(100, 100, 1));
    ASSERT_EQ_INT(-2147483647 - 1, Xp);
    ASSERT_EQ_INT(-2147483647 - 1, Yp);
}

int main(void) {
    g_emit = getenv("LBA2_NUMERIC_EMIT") != NULL;
    if (g_emit) {
        printf("/* Regenerate: LBA2_NUMERIC_EMIT=1 test_numeric_contract */\n");
        test_vector();
        printf("/* %d values */\n", g_idx);
        return 0;
    }

    RUN_TEST(test_vector);
    RUN_TEST(test_distance_properties);
    RUN_TEST(test_rotation_properties);
    RUN_TEST(test_projection_properties);
    TEST_SUMMARY();
    return test_failures != 0;
}
