/* CPU micro-benchmark for the per-frame exterior terrain pass
 * (SOURCES/3DEXT/TERRAIN.CPP AffichageTerrainZBuf), the hottest game-side
 * CPU code on the Quest: one exterior cube is a 65x65 grid of vertices
 * rotated and projected per eye, then 64x64 tiles are classified and
 * radix-sorted before the GL batch is built.
 *
 * Drives the REAL engine math (LongRotatePointF, LongProjectPoint3D,
 * InitMatrixStdF, SetFollowCamera) over a synthetic cube with the same
 * loop shape as AffichageTerrainZBuf, and reports ns per vertex / per tile
 * plus a per-frame estimate at two eyes.  Run it before and after a
 * candidate optimization; the absolute numbers are host-relative, the
 * deltas are what matter.
 *
 * CPP-only, no ASM, no Docker, no game assets.  Build manually:
 *   cmake --build build --target bench_terrain && ./build/tests/bench_terrain/bench_terrain
 */
#include "test_harness.h"

#include "C_EXTERN.H"

#include <3D/CAMERA.H>
#include <3D/DATAMAT.H>
#include <3D/LPROJ.H>

#include <SDL3/SDL.h>

#include <string.h>

#define NB 64                  /* tiles per cube side (NB_COTE) */
#define VERTS_PER_ROW (NB + 1) /* 65 */
#define TOTAL_VERTS (VERTS_PER_ROW * VERTS_PER_ROW)
#define TOTAL_TILES (NB * NB)

static S16 g_heights[TOTAL_VERTS];      /* stands in for MapSommetY */
static U8 g_flags[TOTAL_VERTS];         /* SommetAffichable */
static T_SOMMET_ROT g_rot[TOTAL_VERTS]; /* SommetRot */
static U32 g_keys[TOTAL_TILES];         /* sorted tile keys */

/* Clip globals consumed by the vertex loop live in the game (GLOBAL.CPP);
 * the bench defines its own plausible values. */
S32 ClipZFar = 35700;
S32 ClipXMin = 0, ClipXMax = 1679, ClipYMin = 0, ClipYMax = 1759;

static double now_ms(void) {
    return SDL_GetTicks() * 1.0;
}

/* One transform+project pass over the grid, shaped like the vertex loop in
 * AffichageTerrainZBuf (rotate -> depth -> cone clip -> project -> flag). */
static double bench_vertices(int iters) {
    S16 *ptrm = g_heights;
    Uint64 t0 = SDL_GetPerformanceCounter();
    for (int it = 0; it < iters; it++) {
        U8 *ptra = g_flags;
        T_SOMMET_ROT *ptrs = g_rot;
        ptrm = g_heights;
        for (S32 z = 0; z <= NB * 512; z += 512) {
            for (S32 x = 0; x <= NB * 512; x += 512) {
                S32 zr;
                LongWorldRotatePoint(x, *ptrm, z);
                zr = CameraZr - Z0;
                if (zr <= NearClip) {
                    *ptra = 32;
                } else if ((zr < ClipZFar) AND LongProjectPoint(X0, Y0, Z0)) {
                    U8 flag = 16;
                    if (Xp < ClipXMin)
                        flag |= 1;
                    if (Xp > ClipXMax)
                        flag |= 2;
                    if (Yp < ClipYMin)
                        flag |= 4;
                    if (Yp > ClipYMax)
                        flag |= 8;
                    *ptra = flag;
                    ptrs->Zrot = zr;
                    ptrs->X2D = (S16)Xp;
                    ptrs->Y2D = (S16)Yp;
                } else {
                    *ptra = 0;
                }
                ptra++;
                ptrm++;
                ptrs++;
            }
        }
    }
    Uint64 t1 = SDL_GetPerformanceCounter();
    return (double)(t1 - t0) * 1000.0 / SDL_GetPerformanceFrequency();
}

/* Visibility-mask + tile classification + LSD radix sort, shaped like the
 * tile pass in AffichageTerrainZBuf.  The mask input is the flags the
 * vertex pass just produced, so the two benches chain realistically. */
static double bench_tiles(int iters) {
    Uint64 t0 = SDL_GetPerformanceCounter();
    for (int it = 0; it < iters; it++) {
        S32 tileCount = 0;
        for (S32 z = 0; z < NB; z++) {
            for (S32 x = 0; x < NB; x++) {
                U8 f = g_flags[z * VERTS_PER_ROW + x] &
                       g_flags[z * VERTS_PER_ROW + x + 1] &
                       g_flags[(z + 1) * VERTS_PER_ROW + x] &
                       g_flags[(z + 1) * VERTS_PER_ROW + x + 1];
                if (f == 16) {
                    /* same key packing as the real pass (fog << 24 | banks
                       | classes), with synthetic per-tile fog */
                    S32 fogLevel = (S32)((z * 37 + it * 11) & 15);
                    g_keys[tileCount] =
                        ((U32)fogLevel << 24) | ((U32)((x + z) & 3) << 20) |
                        ((U32)((x * 3 + z) & 3) << 18) |
                        ((U32)((x + z * 5) & 3) << 16) | 0u;
                    tileCount++;
                }
            }
        }
        /* LSD counting sort on the four key bytes (stable). */
        static U32 scratch[TOTAL_TILES];
        static S32 counts[256];
        U32 *src = g_keys;
        U32 *dst = scratch;
        for (int pass = 0; pass < 4; pass++) {
            memset(counts, 0, sizeof(counts));
            for (S32 i = 0; i < tileCount; i++)
                counts[(src[i] >> (pass * 8)) & 0xFF]++;
            S32 sum = 0;
            for (int b = 0; b < 256; b++) {
                S32 c = counts[b];
                counts[b] = sum;
                sum += c;
            }
            for (S32 i = 0; i < tileCount; i++)
                dst[counts[(src[i] >> (pass * 8)) & 0xFF]++] = src[i];
            U32 *t = src;
            src = dst;
            dst = t;
        }
    }
    Uint64 t1 = SDL_GetPerformanceCounter();
    return (double)(t1 - t0) * 1000.0 / SDL_GetPerformanceFrequency();
}

/* ── Distance survey: the old DISTANCE.CPP used sqrtl (software 128-bit
   sqrt on ARM); the replacement is a hardware double sqrt + exact floor
   correction.  Sweep verifies the new result equals floor(sqrt(n)) for
   every input class, then times both. */
static U32 ref_sqrt_floor(U64 n) {
    return (U32)sqrtl((long double)n);
}

static void bench_distance(void) {
    /* Exactness sweep: random sums, perfect squares, squares +-1, and the
       top of the range (the 64-bit sums the S32-squared inputs allow). */
    U64 cases[] = {1, 2, 3, 4, 5, 8, 9, 15, 16, 17, 99, 100, 101,
                   500 * 500, 500 * 500 + 1, 0x7FFFFFFF, 0x80000000ull,
                   0xFFFFFFFFull, 0x100000000ull, 46340ull * 46340ull,
                   46340ull * 46340ull - 1, 46340ull * 46340ull + 1};
    int bad = 0;
    for (U32 i = 0; i < 200000; i++) {
        S32 dx = (S32)rand() % 46341 - 23170;
        S32 dy = (S32)rand() % 46341 - 23170;
        S32 dz = (S32)rand() % 46341 - 23170;
        S64 n = (S64)dx * dx + (S64)dy * dy + (S64)dz * dz;
        if (Distance3D(0, 0, 0, dx, dy, dz) != ref_sqrt_floor((U64)n))
            bad++;
    }
    for (U32 c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        S64 dx = (S64)(S32)cases[c]; /* the engine passes S32 distances */
        if (Distance2D(0, 0, (S32)dx, 0) !=
            ref_sqrt_floor((U64)(dx * dx)))
            bad++;
    }
    printf("distance   : exactness sweep %s\n", bad ? "FAILED" : "OK");

    const int iters = 200000;
    U32 acc = 0;
    Uint64 t0 = SDL_GetPerformanceCounter();
    for (int i = 0; i < iters; i++)
        acc += Distance3D(i * 7, 0, 0, 0, 0, 0);
    Uint64 t1 = SDL_GetPerformanceCounter();
    for (int i = 0; i < iters; i++)
        acc += (U32)sqrtl((long double)(i * 7));
    Uint64 t2 = SDL_GetPerformanceCounter();
    double newMs = (double)(t1 - t0) * 1000.0 / SDL_GetPerformanceFrequency();
    double oldMs = (double)(t2 - t1) * 1000.0 / SDL_GetPerformanceFrequency();
    printf("distance   : %d calls  new %.2f ms  old(sqrtl) %.2f ms  (%.1fx)%s\n",
           iters, newMs, oldMs, oldMs / newMs, acc ? "" : "");
    (void)acc;
}

int main(void) {
    /* Synthetic cube: 65x65 heights in a plausible terrain band. */
    for (int i = 0; i < TOTAL_VERTS; i++)
        g_heights[i] = (S16)(512 + (i * 37) % 4096);

    /* Rig setup mirroring the follow camera over the current cube: camera
     * behind and above the cube centre, looking at it. */
    SetFollowCamera(16384, -8000, 16384, 300, 0, 0, 16000);

    int itersV = 2000;
    int itersT = 20000;

    double msV = bench_vertices(itersV);
    double msT = bench_tiles(itersT);

    double nsPerVert = msV * 1e6 / ((double)itersV * TOTAL_VERTS);
    double nsPerTile = msT * 1e6 / ((double)itersT * TOTAL_TILES);
    /* Per rendered frame the vertex pass runs twice (one per eye) and the
     * tile pass once per eye; the engine measures in whole ms. */
    double frameMs = (msV / itersV) * 2 + (msT / itersT) * 2;

    printf("vertices : %d passes x %d verts = %.2f ms  (%.0f ns/vert)\n",
           itersV, TOTAL_VERTS, msV, nsPerVert);
    printf("tiles    : %d passes x %d tiles = %.2f ms  (%.0f ns/tile)\n",
           itersT, TOTAL_TILES, msT, nsPerTile);
    printf("estimate : %.2f ms per frame for one cube at two eyes\n", frameMs);
    bench_distance();
    printf("           (x9 cubes worst-case on eye0 pre-cull)\n");

    TEST_SUMMARY();
    return test_failures != 0;
}
