// ─────────────────────────────────────────────────────────────────────────────
// tests/render/test_sphere_radius.cpp
//
// Host TDD harness for body-primitive geometry: the sphere-radius projection.
//
// Drives the REAL Sphere() / Sphere_Transp() from LIB386/OBJECT/AFF_OBJ.CPP with
// controlled projected-point state, spies the radius those functions pass to the
// filler, and asserts it equals the value the original AFF_OBJ.ASM computes.
//
// Why this exists (issue #357 - "square billboards larger than retail"):
//   Character eyes are body spheres. Their on-screen radius is derived by
//   Sphere(), independently of the body's vertex projection. The C++ port of the
//   ISO (interior) branch mis-translated the ASM `neg eax` as `radius = ~radius`
//   (bitwise NOT, missing the +1), so every interior sphere came out exactly one
//   pixel too big - invisible on the body silhouette but glaring on 1-3px eyes.
//
// Why the existing tests missed it:
//   The ASM<->CPP polyrec equiv tests compare the *fillers* (Fill_Sphere given a
//   radius). They capture the radius AFTER Sphere() derives it and feed the same
//   value to both paths, so the derivation is never checked. They also only run
//   under 32-bit Docker. This test exercises the derivation on the 64-bit host -
//   the layer nothing else covered.
//
// Extending: Line() and the Triangle_/Quad_ primitives read the same projected-
// point globals and emit Line_A / Fill_Poly. Add a spy + a case the same way to
// grow this into a full body-primitive geometry sweep.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <cstdio>

typedef int32_t S32;
typedef uint32_t U32;
typedef int16_t S16;
typedef uint16_t U16;
typedef uint8_t U8;

// Layouts must match AFF_OBJ.CPP's file-local structs byte-for-byte.
#pragma pack(push, 1)
struct T_OBJ_POINT {
    S16 X, Y, Z, Group;
};
struct TYPE_PT {
    S16 X, Y;
};
struct T_OBJ_SPHERE {
    U16 Type, Coul, P1, Rayon;
};
#pragma pack(pop)

enum { TYPE_3D = 0,
       TYPE_ISO = 1 };

// ── Symbols defined in AFF_OBJ.CPP (compiled into this test) ────────────────
extern S32 Sphere(U32 typePoly, void *poly);
extern S32 Sphere_Transp(U32 typePoly, void *poly);
extern T_OBJ_POINT Obj_ListRotatedPoints[];
extern TYPE_PT Obj_ListProjectedPoints[];
extern S32 PosZWr;

// ── Symbols defined in the real 3D library (linked) ─────────────────────────
extern S32 TypeProj;
extern S32 LFactorX;

// ── pol_work / svga symbols: provided here (spy + stubs) ────────────────────
// Data globals Sphere() reads/writes.
U8 Fill_Flag_ZBuffer = 0;
U8 Fill_Flag_NZW = 0;
U32 Fill_ZBuffer_Factor = 0;
S32 ScreenXMin, ScreenXMax, ScreenYMin, ScreenYMax;
S32 ClipXMin, ClipXMax, ClipYMin, ClipYMax;
S32 RepMask = 0;
U8 *PtrMap = 0;

// Spy: capture the geometry Sphere() hands to the rasteriser.
static int g_fs_calls = 0;
static S32 g_fs_radius = 0;
static S32 g_fs_cx = 0, g_fs_cy = 0, g_fs_type = 0, g_fs_col = 0;

// The pol_work fillers have C linkage (POLY.H wraps them in extern "C").
struct Struc_Point;
extern "C" {
void Fill_Sphere(S32 type, S32 col, S32 cx, S32 cy, S32 rayon, S32 /*z*/) {
    g_fs_calls++;
    g_fs_type = type;
    g_fs_col = col;
    g_fs_cx = cx;
    g_fs_cy = cy;
    g_fs_radius = rayon;
}

// Stubs: referenced by other functions in the AFF_OBJ.CPP TU that this test
// never calls; present only so the translation unit links.
S32 Fill_Poly(S32, S32, S32, Struc_Point *) { return 0; }
void Line_A(S32, S32, S32, S32, S32, S32, S32) {}
}

// ── Oracles: what the original AFF_OBJ.ASM computes ─────────────────────────
//
// ISO branch: `radius *34/512`, then a `neg` that is undone by the common
// `@@Return_Radius` negate - net magnitude 34*Rayon/512.
static S32 asm_iso_radius(S32 rayon) { return (34 * rayon) >> 9; }

// 3D branch: 64-bit `imul LFactorX` / `idiv (Zrot+PosZWr)`, then the
// `@@Return_Radius` negate. 64-bit intermediate matches the ASM edx:eax product.
static S32 asm_3d_radius(S32 rayon, S32 lfactor, S32 zrot, S32 poszwr) {
    long long prod = (long long)rayon * (long long)lfactor;
    long long q = prod / (long long)(zrot + poszwr); // idiv truncates toward zero
    return (S32)(-q);
}

// ── Test scaffolding ────────────────────────────────────────────────────────
static int g_failures = 0;

static void reset_bounds() {
    ScreenXMin = ScreenYMin = 0x7FFFFFFF;
    ScreenXMax = ScreenYMax = (S32)0x80000000;
    Fill_Flag_ZBuffer = 0;
    g_fs_calls = 0;
    g_fs_radius = 0;
}

static void check(const char *what, S32 got, S32 want) {
    if (got != want) {
        printf("  FAIL  %-32s got=%d want=%d\n", what, got, want);
        g_failures++;
    } else {
        printf("  ok    %-32s = %d\n", what, got);
    }
}

// Call the real Sphere() with a synthetic one-sphere body and return the radius
// it emitted to Fill_Sphere.
static S32 emit_sphere_radius(S32 rayon, S32 zrot, bool transp) {
    T_OBJ_SPHERE s;
    s.Type = transp ? 2 : 0;
    s.Coul = 42;
    s.P1 = 0;
    s.Rayon = (U16)rayon;

    Obj_ListRotatedPoints[0].X = 0;
    Obj_ListRotatedPoints[0].Y = 0;
    Obj_ListRotatedPoints[0].Z = (S16)zrot;
    Obj_ListRotatedPoints[0].Group = 0;
    Obj_ListProjectedPoints[0].X = 100; // on-screen centre; irrelevant to radius
    Obj_ListProjectedPoints[0].Y = 100;

    reset_bounds();
    if (transp)
        Sphere_Transp(0, &s);
    else
        Sphere(0, &s);

    if (g_fs_calls != 1) {
        printf("  FAIL  Fill_Sphere call count = %d (expected 1)\n", g_fs_calls);
        g_failures++;
    }
    return g_fs_radius;
}

int main() {
    printf("=== body-primitive geometry: sphere radius ===\n");

    // ── Interior (isometric) spheres: the reported #357 case ────────────────
    // Radius must match retail's 34*Rayon/512 exactly. The port's ISO branch
    // used ~radius instead of -radius, so each of these was 1px too big.
    printf("\n[interior / ISO]  (character eyes; issue #357)\n");
    TypeProj = TYPE_ISO;
    const S32 iso_rayons[] = {15, 30, 60, 100, 200, 400, 800};
    for (unsigned i = 0; i < sizeof(iso_rayons) / sizeof(iso_rayons[0]); ++i) {
        S32 r = iso_rayons[i];
        char label[64];
        snprintf(label, sizeof(label), "iso Rayon=%d -> radius", r);
        check(label, emit_sphere_radius(r, /*zrot*/ 0, /*transp*/ false),
              asm_iso_radius(r));
    }
    // Sphere_Transp shares the identical radius path.
    check("iso transp Rayon=100 -> radius",
          emit_sphere_radius(100, 0, /*transp*/ true), asm_iso_radius(100));

    // ── Exterior (perspective) spheres: the bit-exact path, guard it ────────
    printf("\n[exterior / 3D]  (parity guard; must stay bit-exact)\n");
    TypeProj = TYPE_3D;
    LFactorX = 600; // ChampX, the shipping exterior focal
    struct {
        S32 rayon, zrot, poszwr;
    } cases[] = {
        {100, -100, -200}, // Zrot+PosZWr = -300
        {50, -50, -450},   // -500
        {250, -300, -100}, // -400
        {30, 0, -700},     // -700
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        PosZWr = cases[i].poszwr;
        char label[80];
        snprintf(label, sizeof(label), "3d Rayon=%d z=%d -> radius", cases[i].rayon,
                 cases[i].zrot + cases[i].poszwr);
        check(label, emit_sphere_radius(cases[i].rayon, cases[i].zrot, false),
              asm_3d_radius(cases[i].rayon, LFactorX, cases[i].zrot, cases[i].poszwr));
    }

    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
