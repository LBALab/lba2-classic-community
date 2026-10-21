/* bench_3d_path: what the 3D maths costs, float path against integer path.
 *
 * The float routines (the F files in LIB386/3D) imitate the x87 with long double and
 * lrintl. long double is 80-bit on x86, 64-bit on Apple arm64, MSVC and armv7,
 * and a 128-bit software type on aarch64 Linux and Android, so the same code
 * costs very different amounts per platform. The exterior terrain, sea and sky
 * use the 1997 integer routines (the I files); decors and characters still use the
 * float ones.
 *
 * Prints, per run:
 *   - one exterior frame of terrain vertices (9 cubes of 65 x 65), rotated and
 *     projected through each path, in ms;
 *   - the float routines a body goes through (RotTransList, ProjectList3D,
 *     LightList), in ns per point.
 *
 * Host: build the bench_3d_path target. Phone: scripts/dev/bench-3d-path.sh
 * --android builds it with the NDK for both ABIs and runs it over adb. */

#include <3D/CAMERA.H>
#include <3D/IMATSTD.H>
#include <3D/LITLISTF.H>
#include <3D/LPROJ.H>
#include <3D/LROT3D.H>
#include <3D/PROJ.H>
#include <3D/ROTRALIS.H>

#include <chrono>
#include <cstdio>
#include <cstring>

#ifdef LBA2_BENCH_STANDALONE
/* The NDK build compiles LIB386/3D alone; these are the only outside globals it
   reads (PRLI3DF's screen bounds, MOVE's timer). */
#include <SVGA/SCREENXY.H>
#include <SYSTEM/TIMER.H>
S32 ScreenXMin, ScreenXMax, ScreenYMin, ScreenYMax;
extern "C" {
volatile U32 TimerRefHR;
}
#endif

typedef std::chrono::steady_clock Clock;

static double Ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

static volatile S32 Sink;

static const int kFrames = 100;
static const int kBodyPoints = 100; /* a typical body's point count */
static const int kBodies = 20000;

static void TerrainFrame(int rep) {
    TYPE_MAT mi;
    InitMatrixStdI(&mi, CameraAlpha, CameraBeta, CameraGamma);
    LongRotatePointI(&mi, CameraX, CameraY, CameraZ);
    const S32 cx = X0, cy = Y0, cz = Z0;

    S32 sum = 0;
    Clock::time_point t0 = Clock::now();
    for (int f = 0; f < kFrames; f++)
        for (int c = 0; c < 9; c++)
            for (S32 z = 0; z <= 64 * 512; z += 512)
                for (S32 x = 0; x <= 64 * 512; x += 512) {
                    LongRotatePoint(&MatriceWorld, x - 32768 * (c % 3), (x ^ z) & 1023, z - 32768 * (c / 3));
                    if (LongProjectPoint(X0, Y0, Z0))
                        sum += Xp + Yp;
                }
    Clock::time_point t1 = Clock::now();
    for (int f = 0; f < kFrames; f++)
        for (int c = 0; c < 9; c++)
            for (S32 z = 0; z <= 64 * 512; z += 512)
                for (S32 x = 0; x <= 64 * 512; x += 512) {
                    LongRotatePointI(&mi, x - 32768 * (c % 3), (x ^ z) & 1023, z - 32768 * (c / 3));
                    if (LongProjectPointIRound(X0, Y0, Z0, cx, cy, cz))
                        sum += Xp + Yp;
                }
    Clock::time_point t2 = Clock::now();
    Sink = sum;
    std::printf("run %d  terrain frame: float %.3f ms, integer %.3f ms\n", rep, Ms(t0, t1) / kFrames,
                Ms(t1, t2) / kFrames);
}

static void BodyPoints(int rep) {
    static TYPE_VT16 src[kBodyPoints], rot[kBodyPoints];
    static TYPE_PT pts[kBodyPoints];
    static U16 light[kBodyPoints];
    for (int i = 0; i < kBodyPoints; i++) {
        src[i].X = (S16)((i * 37) % 600 - 300);
        src[i].Y = (S16)((i * 53) % 1200);
        src[i].Z = (S16)((i * 71) % 600 - 300);
        src[i].Grp = 0;
    }

    Clock::time_point t0 = Clock::now();
    for (int b = 0; b < kBodies; b++)
        RotTransList(&MatriceWorld, rot, src, kBodyPoints);
    Clock::time_point t1 = Clock::now();
    for (int b = 0; b < kBodies; b++) {
        std::memcpy(rot, src, sizeof(rot));
        ProjectList3D(pts, rot, kBodyPoints, 16000, 4000, 12000);
    }
    Clock::time_point t2 = Clock::now();
    for (int b = 0; b < kBodies; b++)
        LightList(&MatriceWorld, light, src, kBodyPoints);
    Clock::time_point t3 = Clock::now();
    Sink = pts[7].X + light[7];

    const double n = (double)kBodies * kBodyPoints;
    std::printf("run %d  float, ns per point: RotTransList %.1f, ProjectList3D %.1f, LightList %.1f\n", rep,
                Ms(t0, t1) * 1e6 / n, Ms(t1, t2) * 1e6 / n, Ms(t2, t3) * 1e6 / n);
}

int main() {
    SetProjection(368, 240, 1, 600, 600);
    SetPosCamera(16000, 4000, 16000);
    SetAngleCamera(300, 1024, 0);
    std::printf("bench_3d_path: long double is %d bytes here\n", (int)sizeof(long double));
    for (int rep = 1; rep <= 3; rep++) {
        TerrainFrame(rep);
        BodyPoints(rep);
    }
    return 0;
}
