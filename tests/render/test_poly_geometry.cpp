// ─────────────────────────────────────────────────────────────────────────────
// tests/render/test_poly_geometry.cpp
//
// Body-primitive geometry: the triangle/quad path.
//
//   TestVisible   - backface culling. The cross product
//                   (x2-x1)*(y1-y3) - (y2-y1)*(x1-x3) decides front vs back
//                   (visible when its sign bit is clear); a clipped vertex culls.
//                   Verified against AFF_OBJ.ASM TestVisibleI (same formula, same
//                   sign convention). A flip here drops or double-draws faces.
//   Triangle_Solid - vertex assembly: the projected points are copied into
//                   ListFillPoly and forwarded to Fill_Poly with type = typePoly>>2
//                   and colour = Couleur & 0xFF.
//   Triangle_Flat  - flat shading: colour = (Couleur + (ListLights[Normale]>>8)) & 0xFF,
//                   emitted with poly type 1.
//
// See primitive_harness.h for the harness design.
// ─────────────────────────────────────────────────────────────────────────────
#include "primitive_harness.h"

#include <cstdio>

static void set_proj(int i, S16 x, S16 y) {
    Obj_ListProjectedPoints[i].X = x;
    Obj_ListProjectedPoints[i].Y = y;
}

// Reference cross product, matching TestVisible / AFF_OBJ.ASM TestVisibleI.
static bool ref_visible(S16 x1, S16 y1, S16 x2, S16 y2, S16 x3, S16 y3) {
    S32 value = (x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3);
    return (value & 0x80000000) == 0;
}

int main() {
    printf("=== body primitive: triangle / quad ===\n");
    TypeProj = TYPE_ISO; // z-buffer branch off; endpoints forward verbatim

    // ── Backface culling ────────────────────────────────────────────────────
    printf("\n[TestVisible: winding + clip]\n");
    STRUC_POLY3_ENV tri;
    tri.P1 = 0;
    tri.P2 = 1;
    tri.P3 = 2;

    // Front-facing: cross product >= 0.
    set_proj(0, 0, 0);
    set_proj(1, 0, 10);
    set_proj(2, 10, 0);
    check("front-facing visible", (S32)TestVisible(&tri), 1);
    check("  matches ref cross product", (S32)ref_visible(0, 0, 0, 10, 10, 0), 1);

    // Back-facing: reversed winding, cross product < 0.
    set_proj(0, 0, 0);
    set_proj(1, 10, 0);
    set_proj(2, 0, 10);
    check("back-facing culled", (S32)TestVisible(&tri), 0);
    check("  matches ref cross product", (S32)ref_visible(0, 0, 10, 0, 0, 10), 0);

    // Any clipped vertex culls, regardless of winding.
    set_proj(0, 0, 0);
    set_proj(1, 0, 10);
    set_proj(2, (S16)-0x8000, (S16)-0x8000);
    check("clipped vertex culled", (S32)TestVisible(&tri), 0);

    // ── Triangle_Solid: vertex assembly + type/colour ───────────────────────
    printf("\n[Triangle_Solid: assembly]\n");
    {
        set_proj(0, 11, 22);
        set_proj(1, 33, 44);
        set_proj(2, 55, 66);
        STRUC_POLY3_LIGHT p;
        p.P1 = 0;
        p.P2 = 1;
        p.P3 = 2;
        p.__padding = 0;
        p.Couleur = 0x1234; // & 0xFF -> 0x34
        p.Normale = 0;
        spy_reset();
        Triangle_Solid(/*typePoly*/ 8, &p); // type = 8 >> 2 = 2
        check("poly emitted", g_spy.poly_calls, 1);
        check("poly type (typePoly>>2)", g_spy.poly_type, 2);
        check("poly colour (Couleur & 0xFF)", g_spy.poly_color, 0x34);
        check("poly vertex count", g_spy.poly_nb, 3);
        check("v0.x", ListFillPoly[0].Pt_XE, 11);
        check("v0.y", ListFillPoly[0].Pt_YE, 22);
        check("v1.x", ListFillPoly[1].Pt_XE, 33);
        check("v1.y", ListFillPoly[1].Pt_YE, 44);
        check("v2.x", ListFillPoly[2].Pt_XE, 55);
        check("v2.y", ListFillPoly[2].Pt_YE, 66);
    }

    // ── Triangle_Flat: flat shade colour ────────────────────────────────────
    printf("\n[Triangle_Flat: colour = (Couleur + light) & 0xFF]\n");
    {
        set_proj(0, 1, 2);
        set_proj(1, 3, 4);
        set_proj(2, 5, 6);
        ListLights[3] = 0x0500; // >> 8 -> 5
        STRUC_POLY3_LIGHT p;
        p.P1 = 0;
        p.P2 = 1;
        p.P3 = 2;
        p.__padding = 0;
        p.Couleur = 16;
        p.Normale = 3;
        spy_reset();
        Triangle_Flat(/*typePoly*/ 0, &p);
        check("flat poly type", g_spy.poly_type, 1);
        check("flat colour (16 + 5)", g_spy.poly_color, 21);
    }

    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
