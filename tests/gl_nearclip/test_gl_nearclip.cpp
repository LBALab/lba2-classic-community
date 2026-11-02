/* Unit test for GL_ClipPolyToNear (LIB386/GL/NEARCLIP.CPP).
 *
 * The regression this guards: a body polygon crossing the near plane used to
 * be handed to the software filler, which writes one Log image shared by both
 * eyes in single-pass stereo.  The clipped part therefore kept the eye-0
 * screen position in both halves and slid against the world.  The clip must
 * produce the exact camera-space intersection and project it with the body
 * projection's own arithmetic.
 */
#include <GL/RENDER_GL.H>

#include <3D/CAMERA.H>
#include <POLYGON/POLY.H>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* The renderer globals NEARCLIP.CPP reads.  Defining them here keeps the test
   out of the camera/polygon object files (which drag in the software
   fillers). */
extern "C" {
S32 XCentre = 320;
S32 YCentre = 200;
float FRatioX = 600.0f;
float FRatioY = -1.0f;
U32 Fill_ZBuffer_Factor = 131072;
}

static Struc_Point mkPt(S32 x, S32 y, U16 u, U16 v, U16 light, U16 zo) {
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = (S16)x;
    p.Pt_YE = (S16)y;
    p.Pt_MapU = u;
    p.Pt_MapV = v;
    p.Pt_Light = light;
    p.Pt_ZO = zo;
    return p;
}

/* Independent projection, same expression as ProjectList3DF. */
static void refProject(const S32 cam[3], S32 *sx, S32 *sy, U16 *zo) {
    const S32 z = cam[2];
    const double f = (double)FRatioX / (double)z;
    *sx = XCentre + (S32)lrint((double)cam[0] * f);
    *sy = YCentre + (S32)lrint((double)cam[1] * f * (double)FRatioY);
    *zo = (U16)(((U32)z * Fill_ZBuffer_Factor) >> 16);
}

static void expectProjected(const char *what, const Struc_Point &p,
                            const S32 cam[3]) {
    S32 sx, sy;
    U16 zo;
    refProject(cam, &sx, &sy, &zo);
    if (p.Pt_XE != (S16)sx || p.Pt_YE != (S16)sy || p.Pt_ZO != zo) {
        fprintf(stderr,
                "%s: got (%d,%d,zo=%u), want (%d,%d,zo=%u) for cam (%d,%d,%d)\n",
                what, (int)p.Pt_XE, (int)p.Pt_YE, (unsigned)p.Pt_ZO, (int)sx,
                (int)sy, (unsigned)zo, (int)cam[0], (int)cam[1], (int)cam[2]);
        assert(0);
    }
}

int main(void) {
    /* Case 1: all vertices in front.  Every vertex is re-projected and kept;
       the polygon order is unchanged. */
    {
        const S32 cam[3 * 3] = {100, 50, 2000, -100, 80, 2500, 0, -60, 2200};
        Struc_Point pts[3] = {mkPt(10, 20, 100, 200, 300, 400),
                              mkPt(30, 40, 500, 600, 700, 800),
                              mkPt(50, 60, 900, 1000, 1100, 1200)};
        Struc_Point out[12];
        const S32 m = GL_ClipPolyToNear(cam, pts, 3, 500, out, 12);
        assert(m == 3);
        for (S32 i = 0; i < 3; i++) {
            expectProjected("in-front", out[i], &cam[i * 3]);
            assert(out[i].Pt_MapU == pts[i].Pt_MapU);
            assert(out[i].Pt_Light == pts[i].Pt_Light);
        }
    }

    /* Case 2: one vertex behind the plane.  A triangle becomes a quad; the
       two crossing vertices land on the near plane at the projected
       intersection of each edge. */
    {
        const S32 nearD = 1000;
        const S32 cam[3 * 3] = {0, 0, 2000, 500, 0, 500, -500, 0, 3000};
        Struc_Point pts[3] = {mkPt(0, 0, 0, 0, 100, 0),
                              mkPt(0, 0, 0, 0, 200, 0),
                              mkPt(0, 0, 0, 0, 300, 0)};
        Struc_Point out[12];
        const S32 m = GL_ClipPolyToNear(cam, pts, 3, nearD, out, 12);
        assert(m == 4);

        /* out[0] is the in-front vertex cam[0]; out[1] is the crossing on
           edge 0->1; out[2] is the crossing on edge 1->2; out[3] is the last
           in-front vertex cam[2]. */
        expectProjected("quad v0", out[0], &cam[0]);
        expectProjected("quad v2", out[3], &cam[2 * 3]);

        S32 c01[3];
        const S32 t01num = nearD - cam[2];
        const S32 t01den = cam[3 + 2] - cam[2];
        for (S32 k = 0; k < 3; k++) {
            c01[k] = cam[k] + (S32)(((S64)(cam[3 + k] - cam[k]) * t01num) /
                                    t01den);
        }
        c01[2] = nearD;
        expectProjected("quad v1", out[1], c01);
        /* UVs and light interpolate with the same parameter (1->0.667). */
        assert(out[1].Pt_Light == 166);
        assert(out[1].Pt_MapU == 0);

        S32 c12[3];
        const S32 t12num = nearD - cam[3 + 2];
        const S32 t12den = cam[6 + 2] - cam[3 + 2];
        for (S32 k = 0; k < 3; k++) {
            c12[k] = cam[3 + k] + (S32)(((S64)(cam[6 + k] - cam[3 + k]) *
                                         t12num) /
                                        t12den);
        }
        c12[2] = nearD;
        expectProjected("quad v2", out[2], c12);
        assert(out[2].Pt_Light == 220);

        /* The crossing depth is the near plane. */
        assert(out[1].Pt_ZO == (U16)(((U32)nearD * Fill_ZBuffer_Factor) >> 16));
        assert(out[2].Pt_ZO == (U16)(((U32)nearD * Fill_ZBuffer_Factor) >> 16));
    }

    /* Case 3: entirely behind.  Nothing to submit. */
    {
        const S32 cam[3 * 3] = {0, 0, -100, 10, 0, 50, -10, 0, 200};
        Struc_Point pts[3] = {mkPt(0, 0, 0, 0, 0, 0), mkPt(0, 0, 0, 0, 0, 0),
                              mkPt(0, 0, 0, 0, 0, 0)};
        Struc_Point out[12];
        assert(GL_ClipPolyToNear(cam, pts, 3, 1000, out, 12) == 0);
    }

    /* Case 4: an off-screen in-front vertex is clipped against the safe
       screen edge, so the projected field stays inside S16 and the visible
       part keeps its true slope. */
    {
        const S32 cam[3 * 3] = {100000, 0, 1000, -10, 0, 1000, 0, -10, 1000};
        Struc_Point pts[3] = {mkPt(-32768, -32768, 0, 0, 0, 0),
                              mkPt(0, 0, 0, 0, 0, 0),
                              mkPt(0, 0, 0, 0, 0, 0)};
        Struc_Point out[12];
        const S32 m = GL_ClipPolyToNear(cam, pts, 3, 100, out, 12);
        assert(m == 4);
        for (S32 i = 0; i < m; i++) {
            assert(out[i].Pt_XE <= 30000);
            assert(out[i].Pt_YE <= 30000);
            assert(out[i].Pt_XE >= -30000);
            assert(out[i].Pt_YE >= -30000);
            assert(!(out[i].Pt_XE == -32768 && out[i].Pt_YE == -32768));
        }
        assert(abs(out[0].Pt_XE - 30000) <= 1 || abs(out[1].Pt_XE - 30000) <= 1);
    }

    printf("gl_nearclip: ok\n");
    return 0;
}
