// ─────────────────────────────────────────────────────────────────────────────
// tests/render/primitive_harness.cpp
//
// Definitions for the body-primitive geometry harness: the pol_work stub globals
// the AFF_OBJ.CPP translation unit references, the filler spies that capture the
// emitted draw-call geometry, and the shared reset/assert helpers.
// See primitive_harness.h for the design.
// ─────────────────────────────────────────────────────────────────────────────
#include "primitive_harness.h"

#include <cstdio>

// pol_work / svga data globals the primitives read or update.
U8 Fill_Flag_ZBuffer = 0;
U8 Fill_Flag_NZW = 0;
U32 Fill_ZBuffer_Factor = 0;
S32 ScreenXMin, ScreenXMax, ScreenYMin, ScreenYMax;
S32 ClipXMin, ClipXMax, ClipYMin, ClipYMax;
S32 RepMask = 0;
U8 *PtrMap = 0;

SpyCapture g_spy;
int g_failures = 0;

// The pol_work fillers have C linkage (POLY.H wraps them in extern "C").
extern "C" {
void Fill_Sphere(S32 type, S32 col, S32 cx, S32 cy, S32 rayon, S32 /*z*/) {
    g_spy.sphere_calls++;
    g_spy.sphere_type = type;
    g_spy.sphere_col = col;
    g_spy.sphere_cx = cx;
    g_spy.sphere_cy = cy;
    g_spy.sphere_radius = rayon;
}

void Line_A(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2) {
    g_spy.line_calls++;
    g_spy.line_x0 = x0;
    g_spy.line_y0 = y0;
    g_spy.line_x1 = x1;
    g_spy.line_y1 = y1;
    g_spy.line_col = col;
    g_spy.line_z1 = z1;
    g_spy.line_z2 = z2;
}

S32 Fill_Poly(S32 type, S32 color, S32 nb, Struc_Point *) {
    g_spy.poly_calls++;
    g_spy.poly_type = type;
    g_spy.poly_color = color;
    g_spy.poly_nb = nb;
    return 0;
}
}

void spy_reset() {
    g_spy = SpyCapture(); // value-initialise: zero every field
    ScreenXMin = ScreenYMin = 0x7FFFFFFF;
    ScreenXMax = ScreenYMax = (S32)0x80000000;
    Fill_Flag_ZBuffer = 0;
}

void check(const char *what, S32 got, S32 want) {
    if (got != want) {
        printf("  FAIL  %-40s got=%d want=%d\n", what, got, want);
        g_failures++;
    } else {
        printf("  ok    %-40s = %d\n", what, got);
    }
}
