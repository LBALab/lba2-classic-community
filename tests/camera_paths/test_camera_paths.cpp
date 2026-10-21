/* Host test for the two cameras CAMERA.CPP keeps (LIB386/3D/CAMERA.CPP).
 *
 * The float camera is what every caller has always had, so keeping an integer
 * one beside it must not move it: after each camera call, the float camera and
 * X0, Y0, Z0 are compared with the float routines run alone, as the calls ran
 * before the integer camera existed. The integer camera is compared with the
 * 1997 integer routines run alone, the order a 486 ran them in (the ports are
 * ASM-checked in tests/3D). The two camera paths are compared with calling the
 * routines they stand for. */

#include <3D/CAMERA.H>
#include <3D/DATAMAT.H>
#include <3D/IMATSTD.H>
#include <3D/LIROT3D.H>
#include <3D/LPROJ.H>
#include <3D/LROT3D.H>
#include <3D/PROJ.H>

#include <cstdio>
#include <cstring>

static int failures = 0;

static void Check(const char *what, int i, S32 got, S32 want) {
    if (got == want)
        return;
    if (failures < 20)
        std::printf("FAIL %s (case %d): got %d, want %d\n", what, i, (int)got, (int)want);
    failures++;
}

static U32 rng_state = 0xCA3E7A01u;
static U32 Next() {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state;
}

static S32 Range(S32 lo, S32 hi) {
    return lo + (S32)(Next() % (U32)(hi - lo + 1));
}

struct Want {
    S32 x, y, z;    /* world position */
    S32 xr, yr, zr; /* rotated */
    S32 zrClip;
    S32 x0, y0, z0; /* left in X0, Y0, Z0 */
};

/* SetFollowCamera as it was before the integer camera, on the float routines. */
static Want FloatFollow(S32 tx, S32 ty, S32 tz, S32 a, S32 b, S32 g, S32 zoom) {
    TYPE_MAT m;
    Want w;
    InitMatrixStd(&m, a % 4096, b % 4096, g % 4096);
    LongRotatePoint(&m, tx, ty, tz);
    w.xr = X0;
    w.yr = Y0;
    w.zr = Z0 + zoom;
    w.zrClip = Z0 - NearClip + zoom;
    LongInverseRotatePoint(&m, w.xr, w.yr, w.zr);
    w.x = w.x0 = X0;
    w.y = w.y0 = Y0;
    w.z = w.z0 = Z0;
    return w;
}

/* The same on the 1997 integer routines, as a 486 ran it. */
static Want IntegerFollow(S32 tx, S32 ty, S32 tz, S32 a, S32 b, S32 g, S32 zoom) {
    TYPE_MAT m;
    Want w;
    std::memset(&m, 0, sizeof(m));
    InitMatrixStdI(&m, a % 4096, b % 4096, g % 4096);
    LongRotatePointI(&m, tx, ty, tz);
    w.xr = X0;
    w.yr = Y0;
    w.zr = Z0 + zoom;
    w.zrClip = 0; /* not kept: LongProjectPointIRound clips from CameraZrI */
    LongInverseRotatePointI(&m, w.xr, w.yr, w.zr);
    w.x = w.x0 = X0;
    w.y = w.y0 = Y0;
    w.z = w.z0 = Z0;
    return w;
}

static void CheckFloat(const char *what, int i, const Want &w, int checkX0) {
    char lbl[96];
    std::snprintf(lbl, sizeof lbl, "%s float CameraX", what);
    Check(lbl, i, CameraX, w.x);
    std::snprintf(lbl, sizeof lbl, "%s float CameraY", what);
    Check(lbl, i, CameraY, w.y);
    std::snprintf(lbl, sizeof lbl, "%s float CameraZ", what);
    Check(lbl, i, CameraZ, w.z);
    std::snprintf(lbl, sizeof lbl, "%s float CameraXr", what);
    Check(lbl, i, CameraXr, w.xr);
    std::snprintf(lbl, sizeof lbl, "%s float CameraYr", what);
    Check(lbl, i, CameraYr, w.yr);
    std::snprintf(lbl, sizeof lbl, "%s float CameraZr", what);
    Check(lbl, i, CameraZr, w.zr);
    std::snprintf(lbl, sizeof lbl, "%s float CameraZrClip", what);
    Check(lbl, i, CameraZrClip, w.zrClip);
    if (checkX0) {
        std::snprintf(lbl, sizeof lbl, "%s X0 left as the float path leaves it", what);
        Check(lbl, i, X0, w.x0);
        Check(lbl, i, Y0, w.y0);
        Check(lbl, i, Z0, w.z0);
    }
}

static void CheckInteger(const char *what, int i, const Want &w) {
    char lbl[96];
    std::snprintf(lbl, sizeof lbl, "%s integer CameraXI", what);
    Check(lbl, i, CameraXI, w.x);
    std::snprintf(lbl, sizeof lbl, "%s integer CameraYI", what);
    Check(lbl, i, CameraYI, w.y);
    std::snprintf(lbl, sizeof lbl, "%s integer CameraZI", what);
    Check(lbl, i, CameraZI, w.z);
    std::snprintf(lbl, sizeof lbl, "%s integer CameraXrI", what);
    Check(lbl, i, CameraXrI, w.xr);
    std::snprintf(lbl, sizeof lbl, "%s integer CameraYrI", what);
    Check(lbl, i, CameraYrI, w.yr);
    std::snprintf(lbl, sizeof lbl, "%s integer CameraZrI", what);
    Check(lbl, i, CameraZrI, w.zr);
}

/* The exterior's camera: a follow camera on the hero, then the neighbouring
   cubes' steps and the step back that DrawHorizon2ZBuf takes. */
static void TestFollowAndSteps() {
    SetProjection(320, 240, 1, 600, 600);
    int diverged = 0;
    for (int i = 0; i < 20000; i++) {
        const S32 tx = Range(0, 32767), ty = Range(0, 6000), tz = Range(0, 32767);
        const S32 a = Range(0, 4095), b = Range(0, 4095), g = Range(0, 4095), zoom = Range(500, 46700);

        const Want f = FloatFollow(tx, ty, tz, a, b, g, zoom);
        const Want n = IntegerFollow(tx, ty, tz, a, b, g, zoom);
        SetFollowCamera(tx, ty, tz, a, b, g, zoom);
        CheckFloat("follow", i, f, 1);
        CheckInteger("follow", i, n);
        diverged += (f.x != n.x || f.y != n.y || f.z != n.z);

        /* A step of one cube on each camera from its own position, as the
           integer terrain takes it, and the float camera's step back. */
        const S32 dx = 512 * 64, dz = -512 * 64;
        SetPosCamera(f.x + dx, f.y, f.z + dz);
        SetPosCameraI(n.x + dx, n.y, n.z + dz);
        {
            TYPE_MAT m;
            Want s;
            InitMatrixStd(&m, a % 4096, b % 4096, g % 4096);
            LongRotatePoint(&m, f.x + dx, f.y, f.z + dz);
            s.x = f.x + dx, s.y = f.y, s.z = f.z + dz;
            s.xr = X0, s.yr = Y0, s.zr = Z0, s.zrClip = Z0 - NearClip;
            std::memset(&m, 0, sizeof(m));
            InitMatrixStdI(&m, a % 4096, b % 4096, g % 4096);
            LongRotatePointI(&m, n.x + dx, n.y, n.z + dz);
            Want si;
            si.x = n.x + dx, si.y = n.y, si.z = n.z + dz;
            si.xr = X0, si.yr = Y0, si.zr = Z0;
            CheckFloat("step", i, s, 0);
            CheckInteger("step", i, si);
        }
    }
    /* The integer camera is placed by its own maths, not copied from the float
       one: on some cases they must land on different units, or this test could
       not tell the two apart. */
    std::printf("follow cameras where the integer one lands elsewhere: %d of 20000\n", diverged);
    if (diverged == 0) {
        std::printf("FAIL the integer camera never differs from the float one\n");
        failures++;
    }
}

/* SetAngleCamera turns both cameras about their own positions. */
static void TestAngleKeepsEachPosition() {
    SetProjection(320, 240, 1, 600, 600);
    for (int i = 0; i < 2000; i++) {
        SetFollowCamera(Range(0, 32767), Range(0, 6000), Range(0, 32767), Range(0, 4095), Range(0, 4095), 0,
                        Range(500, 30000));
        const S32 fx = CameraX, fy = CameraY, fz = CameraZ;
        const S32 ix = CameraXI, iy = CameraYI, iz = CameraZI;
        const S32 a = Range(0, 4095), b = Range(0, 4095);
        SetAngleCamera(a, b, 0);
        Check("angle keeps CameraX", i, CameraX, fx);
        Check("angle keeps CameraXI", i, CameraXI, ix);
        TYPE_MAT m;
        std::memset(&m, 0, sizeof(m));
        InitMatrixStdI(&m, a, b, 0);
        LongRotatePointI(&m, ix, iy, iz);
        Check("angle integer CameraXrI", i, CameraXrI, X0);
        Check("angle integer CameraZrI", i, CameraZrI, Z0);
        InitMatrixStd(&m, a, b, 0);
        LongRotatePoint(&m, fx, fy, fz);
        Check("angle float CameraXr", i, CameraXr, X0);
        Check("angle float CameraZr", i, CameraZr, Z0);
    }
}

/* Each path is the routines it names, against the camera it names. */
static void TestPaths() {
    SetProjection(368, 240, 1, 600, 600);
    for (int i = 0; i < 20000; i++) {
        SetFollowCamera(Range(0, 32767), Range(0, 6000), Range(0, 32767), Range(0, 4095), Range(0, 4095), 0,
                        Range(500, 30000));
        const S32 x = Range(-32768, 65535), y = Range(-2000, 8000), z = Range(-32768, 65535);

        CameraPathFloat.WorldRotate(x, y, z);
        const S32 fx0 = X0, fy0 = Y0, fz0 = Z0;
        LongRotatePoint(&MatriceWorld, x, y, z);
        Check("float path rotate", i, fx0, X0);
        Check("float path rotate", i, fy0, Y0);
        Check("float path rotate", i, fz0, Z0);
        const S32 fr = CameraPathFloat.Project(fx0, fy0, fz0);
        const S32 fxp = Xp, fyp = Yp;
        const S32 wr = LongProjectPoint(fx0, fy0, fz0);
        Check("float path project ret", i, fr, wr);
        Check("float path project Xp", i, fxp, Xp);
        Check("float path project Yp", i, fyp, Yp);
        Check("float path camera", i, *CameraPathFloat.Zr, CameraZr);

        CameraPathInteger.WorldRotate(x, y, z);
        const S32 ix0 = X0, iy0 = Y0, iz0 = Z0;
        LongRotatePointI(&MatriceWorldI, x, y, z);
        Check("integer path rotate", i, ix0, X0);
        Check("integer path rotate", i, iy0, Y0);
        Check("integer path rotate", i, iz0, Z0);
        const S32 ir = CameraPathInteger.Project(ix0, iy0, iz0);
        const S32 ixp = Xp, iyp = Yp;
        const S32 jr = LongProjectPointIRound(ix0, iy0, iz0, CameraXrI, CameraYrI, CameraZrI);
        Check("integer path project ret", i, ir, jr);
        Check("integer path project Xp", i, ixp, Xp);
        Check("integer path project Yp", i, iyp, Yp);
        Check("integer path camera", i, *CameraPathInteger.Zr, CameraZrI);
    }
}

int main() {
    TestFollowAndSteps();
    TestAngleKeepsEachPosition();
    TestPaths();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("test_camera_paths: all checks passed\n");
    return 0;
}
