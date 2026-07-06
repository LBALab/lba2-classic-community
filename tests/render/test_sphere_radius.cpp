// ─────────────────────────────────────────────────────────────────────────────
// tests/render/test_sphere_radius.cpp
//
// Body-primitive geometry: the sphere-radius projection (issue #357).
//
// Character eyes are body spheres; their on-screen radius is derived by Sphere()/
// Sphere_Transp() in AFF_OBJ.CPP, independently of the body's vertex projection.
// The ISO (interior) branch mistranslated the original AFF_OBJ.ASM `neg eax` as
// `radius = ~radius` (bitwise NOT, missing the +1), so every interior sphere came
// out one pixel too big. On 1-3px eyes that is dot-vs-blob. The 3D branch has a
// compensating extra negate and is bit-exact.
//
// See primitive_harness.h for why this layer needs host coverage.
// ─────────────────────────────────────────────────────────────────────────────
#include "primitive_harness.h"

#include <cstdio>

// ── Oracles: what the original AFF_OBJ.ASM computes ─────────────────────────
// ISO branch: `radius *34/512`, then a `neg` undone by the common negate.
static S32 asm_iso_radius(S32 rayon) { return (34 * rayon) >> 9; }

// 3D branch: 64-bit `imul LFactorX` / `idiv (Zrot+PosZWr)`, then the negate.
static S32 asm_3d_radius(S32 rayon, S32 lfactor, S32 zrot, S32 poszwr) {
    long long prod = (long long)rayon * (long long)lfactor;
    long long q = prod / (long long)(zrot + poszwr); // idiv truncates toward zero
    return (S32)(-q);
}

// Drive the real Sphere()/Sphere_Transp() with a synthetic one-sphere body and
// return the radius it emitted to the filler.
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

    spy_reset();
    if (transp)
        Sphere_Transp(0, &s);
    else
        Sphere(0, &s);

    if (g_spy.sphere_calls != 1) {
        printf("  FAIL  Fill_Sphere call count = %d (expected 1)\n", g_spy.sphere_calls);
        g_failures++;
    }
    return g_spy.sphere_radius;
}

int main() {
    printf("=== body primitive: sphere radius ===\n");

    // ── Interior (isometric) spheres: the reported #357 case ────────────────
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

    // ── Degenerate: sphere point on the camera Z plane ──────────────────────
    // Zrot + PosZWr == 0. The ASM guards this (@@SkipSphere); the C++ port used
    // to divide by zero here. Expect the sphere to be skipped (no draw call).
    printf("\n[3D degenerate: point on camera Z plane]\n");
    TypeProj = TYPE_3D;
    LFactorX = 600;
    PosZWr = 100;
    {
        T_OBJ_SPHERE s;
        s.Type = 0;
        s.Coul = 1;
        s.P1 = 0;
        s.Rayon = 50;
        Obj_ListRotatedPoints[0].Z = (S16)(-100); // Zrot + PosZWr = 0
        Obj_ListProjectedPoints[0].X = 100;
        Obj_ListProjectedPoints[0].Y = 100;
        spy_reset();
        Sphere(0, &s);
        check("camera-plane sphere skipped (no draw)", g_spy.sphere_calls, 0);
    }

    // ── Characterisation: 3D radius uses a 32-bit multiply ──────────────────
    // The ASM keeps a 64-bit imul/idiv intermediate; the C++ multiplies in S32.
    // At the shipping focal (600) with the largest possible Rayon (U16 max) the
    // product is ~39M, well within S32, so the two agree. This pins that the
    // current code is safe as shipped; a larger focal would need the 64-bit form.
    printf("\n[3D characterisation: 32-bit multiply safe at focal 600]\n");
    TypeProj = TYPE_3D;
    LFactorX = 600;
    PosZWr = -1000;
    check("3d Rayon=65535 (U16 max) matches 64-bit oracle",
          emit_sphere_radius(65535, /*zrot*/ 0, false),
          asm_3d_radius(65535, 600, 0, -1000));

    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
