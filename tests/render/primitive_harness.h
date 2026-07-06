// ─────────────────────────────────────────────────────────────────────────────
// tests/render/primitive_harness.h
//
// Shared scaffolding for host TDD of body-primitive geometry (issue #357 class).
//
// The body renderer's primitives (Sphere, Line, the triangle/quad fillers) each
// take rotated + projected points and compute screen geometry, then emit a draw
// call (Fill_Sphere / Line_A / Fill_Poly). This harness links the REAL primitives
// from LIB386/OBJECT/AFF_OBJ.CPP + the REAL 3D math library, and spies the emitted
// draw calls, so a test can assert the computed geometry against the original
// AFF_OBJ.ASM formula on a 64-bit host.
//
// Why it exists: the ASM/CPP polyrec equivalence tests compare the fillers on
// identical inputs and only run under 32-bit Docker, so the geometry each
// primitive DERIVES is never checked on the host. That is where #357 (interior
// sphere radius off by one) hid. Add a primitive by writing a small test that
// sets up projected points, calls the primitive, and reads g_spy.
//
// Each test executable links [<test>.cpp + primitive_harness.cpp + AFF_OBJ.CPP]
// against the 3D library; the spies and pol_work stub globals live in
// primitive_harness.cpp so no framebuffer, palette, or game data is needed.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef PRIMITIVE_HARNESS_H
#define PRIMITIVE_HARNESS_H

#include <cstdint>

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
struct T_OBJ_LINE {
    U16 Type, Coul, P1, P2;
};
struct Struc_Point {
    S16 Pt_XE, Pt_YE;
    U16 Pt_MapU, Pt_MapV, Pt_Light, Pt_ZO;
    S32 Pt_W;
};
struct STRUC_POLY3_LIGHT {
    U16 P1, P2, P3, __padding, Couleur, Normale;
};
struct STRUC_POLY4_LIGHT {
    U16 P1, P2, P3, P4, Couleur, Normale;
};
struct STRUC_POLY3_ENV {
    U16 P1, P2, P3, HandleEnv, Couleur, Normale, Scale, __padding;
};
#pragma pack(pop)

enum { TYPE_3D = 0,
       TYPE_ISO = 1 };

// ── Real code under test (AFF_OBJ.CPP, compiled into each executable) ────────
extern S32 Sphere(U32 typePoly, void *poly);
extern S32 Sphere_Transp(U32 typePoly, void *poly);
extern S32 Line(U32 typePoly, void *poly);
extern S32 Triangle_Solid(U32 typePoly, void *poly);
extern S32 Triangle_Flat(U32 typePoly, void *poly);
extern S32 Quad_Solid(U32 typePoly, void *poly);
extern bool TestVisible(STRUC_POLY3_ENV *poly);
extern T_OBJ_POINT Obj_ListRotatedPoints[];
extern TYPE_PT Obj_ListProjectedPoints[];
extern Struc_Point ListFillPoly[]; // AFF_OBJ.CPP scratch: assembled poly vertices
extern U16 ListLights[];           // per-normal light intensities
extern S32 PosZWr;

// ── Real 3D library (linked) ─────────────────────────────────────────────────
extern S32 TypeProj;
extern S32 LFactorX;

// ── pol_work globals the primitives read (defined in primitive_harness.cpp) ──
extern U8 Fill_Flag_ZBuffer;
extern U32 Fill_ZBuffer_Factor;

// ── Captured draw calls (populated by the spies in primitive_harness.cpp) ────
struct SpyCapture {
    int sphere_calls;
    S32 sphere_radius, sphere_cx, sphere_cy, sphere_type, sphere_col;
    int line_calls;
    S32 line_x0, line_y0, line_x1, line_y1, line_col;
    int poly_calls;
    S32 poly_type, poly_color, poly_nb;
};
extern SpyCapture g_spy;

// Clear the capture, reset the 2D-bbox globals, and disable the z-buffer flag.
void spy_reset();

// ── Assertion helper ─────────────────────────────────────────────────────────
extern int g_failures;
void check(const char *what, S32 got, S32 want);

#endif // PRIMITIVE_HARNESS_H
