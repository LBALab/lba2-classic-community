// ──────────────────────────────────────────────────────────────────────────
// Polygon triangulation for the GPU renderer.
// ──────────────────────────────────────────────────────────────────────────
#include <GL/RENDER_GL.H>

// Ear-clip workspace cap: engine polygons are quads (occasionally small
// N-gons produced by clipping); anything larger falls back to a fan.
#define GL_TRI_MAX_VERTS 64

/* Triangulate the screen-space polygon `pts` (n points) into index triples
   written to `out` (capacity 3*(n-2)); returns the number of indices written.

   The SW scanline fillers cover exactly the polygon outline.  A triangle fan
   from vertex 0 does not: when the reflex vertex of a concave projection
   (e.g. a non-planar quad seen edge-on) sits on the fan apex, the fan
   triangles reach outside the outline and spill this polygon's shade over
   its neighbours.  Ear clipping triangulates with interior diagonals only,
   so the GPU fills the same area the SW rasterizer would. */
S32 GL_TriangulatePolygon(const Struc_Point *pts, S32 n, U16 *out) {
    S32 nout = 0;
    S32 i;

    if (n < 3) {
        return 0;
    }
    if (n > GL_TRI_MAX_VERTS) {
        for (i = 1; i < n - 1; i++) {
            out[nout++] = 0;
            out[nout++] = (U16)i;
            out[nout++] = (U16)(i + 1);
        }
        return nout;
    }

    /* Signed area fixes the winding; an ear must be convex in that
       direction. */
    S64 area2 = 0;
    for (i = 0; i < n; i++) {
        S32 j = (i + 1 < n) ? i + 1 : 0;
        area2 += (S64)pts[i].Pt_XE * pts[j].Pt_YE - (S64)pts[j].Pt_XE * pts[i].Pt_YE;
    }
    S32 winding = (area2 < 0) ? -1 : 1;

    U16 idx[GL_TRI_MAX_VERTS];
    S32 count = n;
    for (i = 0; i < n; i++) {
        idx[i] = (U16)i;
    }

    /* Collinear corners pass the convex test with a zero-area triangle —
       they rasterize nothing and keep the clip loop making progress.  The
       guard plus the fan fallback below absorb degenerate inputs (duplicate
       points) that would otherwise spin forever. */
    S32 guard = 0;
    while (count > 3 && guard < n * n) {
        guard++;
        S32 clipped = 0;
        for (i = 0; i < count; i++) {
            S32 ip = (i + count - 1) % count;
            S32 in = (i + 1) % count;
            S32 a = idx[ip];
            S32 b = idx[i];
            S32 c = idx[in];
            S64 ax = pts[a].Pt_XE, ay = pts[a].Pt_YE;
            S64 bx = pts[b].Pt_XE, by = pts[b].Pt_YE;
            S64 cx = pts[c].Pt_XE, cy = pts[c].Pt_YE;
            S64 cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (cross * winding < 0) {
                continue; /* reflex corner: not an ear */
            }
            /* Skip the ear when any other vertex lies inside or on the
               candidate triangle — that diagonal would cut off part of the
               polygon. */
            S32 contained = 0;
            for (S32 k = 0; k < count; k++) {
                if (k == ip || k == i || k == in) {
                    continue;
                }
                S64 px = pts[idx[k]].Pt_XE, py = pts[idx[k]].Pt_YE;
                S64 d0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
                S64 d1 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
                S64 d2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
                if (d0 * winding >= 0 && d1 * winding >= 0 &&
                    d2 * winding >= 0) {
                    contained = 1;
                    break;
                }
            }
            if (contained) {
                continue;
            }
            out[nout++] = (U16)a;
            out[nout++] = (U16)b;
            out[nout++] = (U16)c;
            for (S32 k = i; k < count - 1; k++) {
                idx[k] = idx[k + 1];
            }
            count--;
            clipped = 1;
            break;
        }
        if (!clipped) {
            break; /* numeric dead end: fan the remainder below */
        }
    }

    if (count == 3) {
        out[nout++] = idx[0];
        out[nout++] = idx[1];
        out[nout++] = idx[2];
    } else {
        for (i = 1; i < count - 1; i++) {
            out[nout++] = idx[0];
            out[nout++] = idx[i];
            out[nout++] = idx[i + 1];
        }
    }
    return nout;
}
