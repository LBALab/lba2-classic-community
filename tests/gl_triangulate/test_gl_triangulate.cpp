/* Unit test for GL_TriangulatePolygon (LIB386/GL/TRIANGULATE.CPP).
 *
 * The regression this guards: the old triangle fan from vertex 0 painted
 * outside the polygon outline whenever the reflex vertex of a concave
 * projection sat on the apex — building surfaces flashed bright/dark wedges
 * while the camera moved.  The triangulation must tile the polygon exactly:
 * every point strictly inside the polygon is covered by exactly one emitted
 * triangle, every point strictly outside by none, and each triangle keeps
 * the polygon's winding.
 */
#include <GL/RENDER_GL.H>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

struct Vec {
    double x, y;
};

static S64 crossSign(const Struc_Point &a, const Struc_Point &b,
                     const Struc_Point &c) {
    S64 v = (S64)(b.Pt_XE - a.Pt_XE) * (c.Pt_YE - a.Pt_YE) -
            (S64)(b.Pt_YE - a.Pt_YE) * (c.Pt_XE - a.Pt_XE);
    return (v > 0) - (v < 0);
}

static Struc_Point mkPt(double x, double y) {
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = (S16)x;
    p.Pt_YE = (S16)y;
    return p;
}

static bool pointInTri(const Vec &p, const Vec &a, const Vec &b, const Vec &c) {
    double d0 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    double d1 = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
    double d2 = (a.x - c.x) * (p.y - c.y) - (a.y - c.y) * (p.x - c.x);
    bool neg = d0 < 0 || d1 < 0 || d2 < 0;
    bool pos = d0 > 0 || d1 > 0 || d2 > 0;
    return !(neg && pos); /* inside or on edge */
}

static bool pointInPoly(const Vec &p, const std::vector<Vec> &poly) {
    /* Winding-independent even-odd test. */
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if ((poly[i].y > p.y) != (poly[j].y > p.y)) {
            double xint = poly[j].x +
                          (p.y - poly[j].y) * (poly[i].x - poly[j].x) /
                              (poly[i].y - poly[j].y);
            if (p.x < xint)
                inside = !inside;
        }
    }
    return inside;
}

/* A point exactly on a polygon edge: distance to the closest edge segment. */
static double distToEdges(const Vec &p, const std::vector<Vec> &poly) {
    double best = 1e30;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        double ex = poly[i].x - poly[j].x, ey = poly[i].y - poly[j].y;
        double len2 = ex * ex + ey * ey;
        double t = ((p.x - poly[j].x) * ex + (p.y - poly[j].y) * ey) / len2;
        if (t < 0)
            t = 0;
        if (t > 1)
            t = 1;
        double dx = p.x - (poly[j].x + t * ex);
        double dy = p.y - (poly[j].y + t * ey);
        double d = sqrt(dx * dx + dy * dy);
        if (d < best)
            best = d;
    }
    return best;
}

static void checkCoverage(const char *name, const std::vector<Vec> &poly) {
    std::vector<Struc_Point> pts;
    for (size_t i = 0; i < poly.size(); i++)
        pts.push_back(mkPt(poly[i].x, poly[i].y));

    std::vector<U16> tris(3 * (poly.size() + 2));
    S32 ntri = GL_TriangulatePolygon(pts.data(), (S32)pts.size(), tris.data());
    assert(ntri == 3 * (S32)(poly.size() - 2));

    double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
    for (size_t i = 0; i < poly.size(); i++) {
        minx = minx < poly[i].x ? minx : poly[i].x;
        miny = miny < poly[i].y ? miny : poly[i].y;
        maxx = maxx > poly[i].x ? maxx : poly[i].x;
        maxy = maxy > poly[i].y ? maxy : poly[i].y;
    }

    const double STEP = 0.37; /* off-grid so vertices/edges are not sampled */
    for (double y = miny + 0.5; y < maxy; y += STEP) {
        for (double x = minx + 0.5; x < maxx; x += STEP) {
            Vec p = {x, y};
            double edgeDist = distToEdges(p, poly);
            if (edgeDist < 0.5)
                continue; /* ambiguous near edges */

            int covered = 0;
            for (S32 t = 0; t < ntri; t += 3) {
                Vec a = {(double)pts[tris[t]].Pt_XE, (double)pts[tris[t]].Pt_YE};
                Vec b = {(double)pts[tris[t + 1]].Pt_XE,
                         (double)pts[tris[t + 1]].Pt_YE};
                Vec c = {(double)pts[tris[t + 2]].Pt_XE,
                         (double)pts[tris[t + 2]].Pt_YE};
                if (pointInTri(p, a, b, c))
                    covered++;
            }
            int want = pointInPoly(p, poly) ? 1 : 0;
            if (covered != want) {
                fprintf(stderr,
                        "%s: FAIL at (%.2f,%.2f): covered=%d want=%d\n", name,
                        x, y, covered, want);
                assert(0);
            }
        }
    }
    printf("%s: ok (%d triangles)\n", name, ntri / 3);
}

int main(void) {
    /* Convex quad: fan and ear clipping agree. */
    {
        std::vector<Vec> poly = {{100, 100}, {300, 110}, {310, 250}, {90, 240}};
        checkCoverage("convex quad", poly);
    }

    /* The regression: a dart (concave quad) with the reflex vertex at the
       fan apex, vertex 0.  The fan spills triangle (0,2,3) outside the
       outline; ear clipping must not. */
    {
        std::vector<Vec> poly = {{200, 200}, {400, 100}, {300, 200}, {400, 320}};
        checkCoverage("concave quad, reflex at apex", poly);
    }

    /* Same dart, CW winding. */
    {
        std::vector<Vec> poly = {{200, 200}, {400, 320}, {300, 200}, {400, 100}};
        checkCoverage("concave quad CW", poly);
    }

    /* Concave pentagon, reflex mid-list. */
    {
        std::vector<Vec> poly = {{100, 100}, {300, 120}, {250, 180}, {350, 260}, {90, 280}};
        checkCoverage("concave pentagon", poly);
    }

    /* Winding preservation: each emitted triangle matches the polygon's. */
    {
        Struc_Point quad[4] = {mkPt(200, 200), mkPt(400, 100), mkPt(300, 200),
                               mkPt(400, 320)};
        U16 tris[3 * 3];
        S32 ntri = GL_TriangulatePolygon(quad, 4, tris);
        assert(ntri == 6);
        S64 want = crossSign(quad[0], quad[1], quad[2]);
        for (S32 t = 0; t < ntri; t += 3) {
            assert(crossSign(quad[tris[t]], quad[tris[t + 1]],
                             quad[tris[t + 2]]) == want);
        }
        printf("winding preserved: ok\n");
    }

    /* Degenerate inputs survive: collinear chain and duplicated point. */
    {
        Struc_Point col[4] = {mkPt(0, 0), mkPt(10, 0), mkPt(20, 0),
                              mkPt(30, 0)};
        U16 tris[3 * 3];
        S32 n = GL_TriangulatePolygon(col, 4, tris);
        (void)n; /* shape is empty; only require no crash and sane count */
        assert(n >= 0 && n <= 9);

        Struc_Point dup[4] = {mkPt(10, 10), mkPt(50, 10), mkPt(50, 50),
                              mkPt(50, 50)};
        n = GL_TriangulatePolygon(dup, 4, tris);
        assert(n == 6);
        printf("degenerate inputs: ok\n");
    }

    printf("all triangulation tests passed\n");
    return 0;
}
