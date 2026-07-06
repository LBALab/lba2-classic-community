// ─────────────────────────────────────────────────────────────────────────────
// tests/render/test_line_geometry.cpp
//
// Body-primitive geometry: the Line primitive.
//
// Line() reads the two already-projected endpoints, drops the primitive when
// either endpoint is the projection's clip sentinel (-0x8000, -0x8000), and
// otherwise emits Line_A(x0, y0, x1, y1, colour, ...). This pins the common path
// on the host: correct endpoints forwarded, and the clip-skip honoured.
//
// The 3D z-buffer branch (Line's z1/z2 derivation) is deliberately not asserted
// yet: it is only reached for perspective + z-buffer lines and its z1/z2 vs P1/P2
// pairing needs Line_A's register convention decoded against the ASM. Follow-up.
//
// See primitive_harness.h for the harness design.
// ─────────────────────────────────────────────────────────────────────────────
#include "primitive_harness.h"

#include <cstdio>

static void set_point(int i, S16 px, S16 py, S16 pz) {
    Obj_ListProjectedPoints[i].X = px;
    Obj_ListProjectedPoints[i].Y = py;
    Obj_ListRotatedPoints[i].Z = pz;
    Obj_ListRotatedPoints[i].Group = 0;
}

int main() {
    printf("=== body primitive: line endpoints ===\n");

    // ISO so the 3D z-buffer branch is not taken; endpoints forward verbatim.
    TypeProj = TYPE_ISO;

    printf("\n[normal line -> Line_A with projected endpoints]\n");
    {
        T_OBJ_LINE l;
        l.Type = 0;
        l.Coul = 7;
        l.P1 = 0;
        l.P2 = 1;
        set_point(0, 10, 20, 0);
        set_point(1, 100, 120, 0);
        spy_reset();
        Line(0, &l);
        check("line emitted", g_spy.line_calls, 1);
        check("line x0", g_spy.line_x0, 10);
        check("line y0", g_spy.line_y0, 20);
        check("line x1", g_spy.line_x1, 100);
        check("line y1", g_spy.line_y1, 120);
        check("line colour", g_spy.line_col, 7);
    }

    printf("\n[clipped endpoint (-0x8000) -> skipped]\n");
    {
        T_OBJ_LINE l;
        l.Type = 0;
        l.Coul = 7;
        l.P1 = 0;
        l.P2 = 1;
        set_point(0, (S16)-0x8000, (S16)-0x8000, 0); // projection clip sentinel
        set_point(1, 50, 60, 0);
        spy_reset();
        Line(0, &l);
        check("clipped P1 suppresses draw", g_spy.line_calls, 0);
    }
    {
        T_OBJ_LINE l;
        l.Type = 0;
        l.Coul = 7;
        l.P1 = 0;
        l.P2 = 1;
        set_point(0, 30, 40, 0);
        set_point(1, (S16)-0x8000, (S16)-0x8000, 0);
        spy_reset();
        Line(0, &l);
        check("clipped P2 suppresses draw", g_spy.line_calls, 0);
    }

    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
