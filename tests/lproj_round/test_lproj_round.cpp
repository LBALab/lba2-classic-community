/* Host test for LongProjectPointIRound (LIB386/3D/LPROJ3DI.CPP).
 *
 * The exterior terrain, sea and sky project with it: the 1997 integer
 * projection (LongProjectPoint3DI, whose ASM equivalence is in tests/3D) with
 * the quotient rounded to nearest, ties to even, instead of truncated. Checks:
 * exact ties go to even; wherever the division is exact it agrees with the
 * truncating port; the clip branch is the same, and with no near clip a point
 * at or behind the camera is clipped rather than divided; and against the float
 * projection the objects use it is never more than a pixel away, and lands on
 * the same pixel nearly always. */

#include <3D/CAMERA.H>
#include <3D/LPROJ.H>
#include <3D/PROJ.H>

#include <cstdio>
#include <cstdlib>

static int failures = 0;

static void Fail(const char *what, S32 x, S32 y, S32 z, S32 got, S32 want) {
    if (failures < 20)
        std::printf("FAIL %s x=%d y=%d z=%d: got %d, want %d\n", what, (int)x, (int)y, (int)z, (int)got, (int)want);
    failures++;
}

static U32 rng_state = 0x5EA5EA5Eu;
static U32 Next() {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state;
}

/* Camera at the origin, centre 0: Xp is -x * fx / dz, Yp is y * fy / dz. */
static void CameraAtOrigin(S32 fx, S32 fy) {
    SetProjection(0, 0, 1, fx, fy);
    CameraXr = CameraYr = CameraZr = 0;
    CameraZrClip = -1;
}

static void TestTies() {
    CameraAtOrigin(1, 1);
    /* y / dz with dz = -2: 5/-2 = -2.5 -> -2, 7/-2 = -3.5 -> -4, -5/-2 = 2.5 -> 2,
       -7/-2 = 3.5 -> 4, 3/-2 = -1.5 -> -2, 1/-2 = -0.5 -> 0. */
    const S32 cases[][2] = {{5, -2}, {7, -4}, {-5, 2}, {-7, 4}, {3, -2}, {1, 0}, {-1, 0}, {4, -2}};
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        LongProjectPointIRound(0, cases[i][0], -2, 0, 0, 0);
        if (Yp != cases[i][1])
            Fail("tie", 0, cases[i][0], -2, Yp, cases[i][1]);
    }
    /* Not a tie: 7/-3 = -2.33 -> -2, 8/-3 = -2.67 -> -3. */
    LongProjectPointIRound(0, 7, -3, 0, 0, 0);
    if (Yp != -2)
        Fail("near", 0, 7, -3, Yp, -2);
    LongProjectPointIRound(0, 8, -3, 0, 0, 0);
    if (Yp != -3)
        Fail("near", 0, 8, -3, Yp, -3);
}

static void TestClipAndExact(U32 seed) {
    rng_state = seed;
    SetProjection(368, 240, 5, 600, 600);
    CameraXr = 1200;
    CameraYr = -800;
    CameraZr = 20000;
    CameraZrClip = CameraZr - NearClip;
    for (int i = 0; i < 200000; i++) {
        /* One case in seven sits at the clip: dz from 0 to -9, where up to -4
           is inside it (z > CameraZr - NearClip) and from -5 projects. */
        const S32 dz = (i % 7 == 0) ? -(S32)(Next() % 10u) : -(S32)(1 + Next() % 50000u);
        const S32 kx = (S32)(Next() % 400u) - 200;
        const S32 ky = (S32)(Next() % 400u) - 200;
        /* An offset of k * dz makes the quotient -k * 600 exactly, on both
           axes, so rounding and truncation must agree wherever it projects. */
        const S32 x = CameraXr + kx * dz;
        const S32 y = CameraYr + ky * dz;
        const S32 z = CameraZr + dz;

        const S32 retT = LongProjectPoint3DI(x, y, z);
        const S32 xpT = Xp, ypT = Yp;
        const S32 retR = LongProjectPointIRound(x, y, z, CameraXr, CameraYr, CameraZr);
        if (retR != retT)
            Fail("clip ret", x, y, z, retR, retT);
        if (Xp != xpT)
            Fail("exact Xp", x, y, z, Xp, xpT);
        if (Yp != ypT)
            Fail("exact Yp", x, y, z, Yp, ypT);
    }
}

/* With no near clip (isometric mode leaves NearClip at 0), a point at the
   camera's depth or behind it would divide by zero or by a positive depth: it
   is clipped instead, as the 3D clip would have it. */
static void TestNoNearClip() {
    SetProjection(368, 240, 0, 600, 600);
    const S32 zs[] = {20000, 20001, 30000};
    for (unsigned i = 0; i < sizeof(zs) / sizeof(zs[0]); i++) {
        const S32 ret = LongProjectPointIRound(1300, -700, zs[i], 1200, -800, 20000);
        if (ret != 0 || Xp != -2147483647 - 1 || Yp != -2147483647 - 1)
            Fail("no near clip", 1300, -700, zs[i], ret, 0);
    }
    const S32 ret = LongProjectPointIRound(1300, -700, 19999, 1200, -800, 20000);
    if (ret != 1)
        Fail("no near clip, just in front", 1300, -700, 19999, ret, 1);
}

/* Against the float projection objects use: same point, same camera. */
static void TestAgainstFloat() {
    SetProjection(368, 240, 1, 600, 600);
    int same = 0, total = 0;
    for (int i = 0; i < 200000; i++) {
        CameraXr = (S32)(Next() % 200000u) - 100000;
        CameraYr = (S32)(Next() % 40000u) - 20000;
        CameraZr = (S32)(Next() % 200000u) - 100000;
        CameraZrClip = CameraZr - NearClip;
        const S32 x = CameraXr + (S32)(Next() % 60000u) - 30000;
        const S32 y = CameraYr + (S32)(Next() % 20000u) - 10000;
        const S32 z = CameraZr - 200 - (S32)(Next() % 60000u);

        if (!LongProjectPoint3D(x, y, z))
            continue;
        const S32 xpF = Xp, ypF = Yp;
        if (!LongProjectPointIRound(x, y, z, CameraXr, CameraYr, CameraZr)) {
            Fail("float projects, integer clips", x, y, z, 0, 1);
            continue;
        }
        if (std::abs(Xp - xpF) > 1)
            Fail("Xp vs float", x, y, z, Xp, xpF);
        if (std::abs(Yp - ypF) > 1)
            Fail("Yp vs float", x, y, z, Yp, ypF);
        same += (Xp == xpF && Yp == ypF);
        total++;
    }
    std::printf("same pixel as the float projection: %d of %d\n", same, total);
    if (total == 0 || same < total * 999 / 1000) {
        std::printf("FAIL fewer than 99.9%% on the float projection's pixel\n");
        failures++;
    }
}

int main() {
    TestTies();
    /* Exact by construction, so any seed holds: three show it is not one. */
    TestClipAndExact(0x5EA5EA5Eu);
    TestClipAndExact(0x12345679u);
    TestClipAndExact(0xC0FFEE01u);
    TestNoNearClip();
    TestAgainstFloat();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("test_lproj_round: all checks passed\n");
    return 0;
}
