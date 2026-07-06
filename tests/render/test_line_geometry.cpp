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
// The 3D z-buffer branch derives per-endpoint depths z1 (from P1) and z2 (from
// P2). AFF_OBJ.ASM loads P1's z into esi and P2's z into edi before Line_A
// (whose convention is eax=x0, ebx=y0, ecx=x1, edx=y1, ebp=col, esi=z1, edi=z2),
// so z1 pairs with P1. A stale comment in Line() claimed the opposite; this test
// pins the correct pairing.
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

// ASM Line z-buffer depth: (Fill_ZBuffer_Factor * (-rotatedZ - PosZWr)) >> 16.
static S32 asm_line_z(S32 rotatedZ, S32 poszwr, U32 factor) {
    U32 depth = (U32)(-rotatedZ - poszwr);
    return (S32)((factor * depth) >> 16);
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

    // ── 3D z-buffer: per-endpoint depths, z1 from P1 and z2 from P2 ──────────
    printf("\n[3D z-buffer: z1<-P1, z2<-P2 (not swapped)]\n");
    TypeProj = TYPE_3D;
    {
        T_OBJ_LINE l;
        l.Type = 0;
        l.Coul = 7;
        l.P1 = 0;
        l.P2 = 1;
        set_point(0, 10, 20, /*rotZ*/ -100);
        set_point(1, 100, 120, /*rotZ*/ -50);
        PosZWr = -200;
        spy_reset();
        Fill_Flag_ZBuffer = 1;         // enable the z-buffer branch (after reset)
        Fill_ZBuffer_Factor = 0x20000; // 2.0 in 16.16
        Line(0, &l);
        check("line emitted", g_spy.line_calls, 1);
        check("z1 depth from P1", g_spy.line_z1, asm_line_z(-100, -200, 0x20000));
        check("z2 depth from P2", g_spy.line_z2, asm_line_z(-50, -200, 0x20000));
    }

    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
