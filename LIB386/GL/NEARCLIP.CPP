/* Near-plane and off-screen clipping for the software-projected body
 * polygons.
 *
 * The SW projection (ProjectList3DF / GetZO3) replaces the screen position of
 * a body vertex that is closer than NearClip or beyond the S16 screen field
 * with the (-32768,-32768) sentinel.  The original engine hands those
 * polygons to the SW filler, which clips them against the screen rect.  In
 * single-pass stereo that filler writes into one Log image shared by both
 * eyes, so the polygon keeps the eye-0 screen position in both halves and
 * slides against the world.
 *
 * Clip the polygon in camera space instead, where every edge is straight and
 * the attributes interpolate linearly, and project the result so the GL batch
 * can place it per eye.  Five half-spaces bound the result: the near plane
 * and the four safe screen edges.  Pure geometry: no GL calls, no renderer
 * state except the projection constants the body projection already uses. */

#include <GL/RENDER_GL.H>

#include <3D/CAMERA.H>
#include <POLYGON/POLY.H>

#include <math.h>

/* Screen field the clipped polygon is kept inside.  S16 has 32767 to spare;
   the margin keeps the rasterizer at an exact, portable coordinate. */
#define CLIP_SCREEN_BOUND 30000

typedef struct {
    S32 x;
    S32 y;
    S32 z;
    Struc_Point p;
} T_CLIP_VERT;

/* Project one camera-space vertex with the body projection.  Callers only
   pass vertices already inside CLIP_SCREEN_BOUND, so the S16 store is exact. */
static void GL_ProjectPolyCam(const S32 *cam, Struc_Point *out) {
    const S32 z = cam[2];
    const double dz = (double)(z > 0 ? z : 1);
    const S32 px =
        XCentre + (S32)lrint((double)cam[0] * (double)FRatioX / dz);
    const S32 py =
        YCentre +
        (S32)lrint((double)cam[1] * (double)FRatioX * (double)FRatioY / dz);
    out->Pt_XE = (S16)px;
    out->Pt_YE = (S16)py;
    out->Pt_ZO = (U16)(((U32)z * Fill_ZBuffer_Factor) >> 16);
}

static S32 InterpAttr(S32 a, S32 b, S64 num, S64 den) {
    return a + (S32)(((S64)(b - a) * num) / den);
}

/* Sutherland-Hodgman against a*x + b*y + c*z + d >= 0.  The plane
   coefficients are integers because the projection factors are integral
   (FRatioX = LFactorX, FRatioX*FRatioY = -LFactorY), so the crossing
   parameter stays exact. */
static S32 ClipAgainstPlane(const T_CLIP_VERT *in, S32 n, S64 a, S64 b,
                            S64 c, S64 d, T_CLIP_VERT *out, S32 outCap) {
    S32 m = 0;

    for (S32 i = 0; i < n; i++) {
        const S32 j = (i + 1) % n;
        const T_CLIP_VERT *pi = &in[i];
        const T_CLIP_VERT *pj = &in[j];
        const S64 di = a * pi->x + b * pi->y + c * pi->z + d;
        const S64 dj = a * pj->x + b * pj->y + c * pj->z + d;
        const S32 inI = (di >= 0);
        const S32 inJ = (dj >= 0);

        if (inI && m < outCap) {
            out[m++] = *pi;
        }
        if (inI != inJ) {
            const S64 den = di - dj;
            if (den == 0) {
                continue;
            }
            const S64 num = di;
            T_CLIP_VERT v;
            v.x = pi->x + (S32)(((S64)(pj->x - pi->x) * num) / den);
            v.y = pi->y + (S32)(((S64)(pj->y - pi->y) * num) / den);
            v.z = pi->z + (S32)(((S64)(pj->z - pi->z) * num) / den);
            v.p = pi->p;
            v.p.Pt_MapU =
                (U16)InterpAttr(pi->p.Pt_MapU, pj->p.Pt_MapU, num, den);
            v.p.Pt_MapV =
                (U16)InterpAttr(pi->p.Pt_MapV, pj->p.Pt_MapV, num, den);
            v.p.Pt_Light =
                (U16)InterpAttr(pi->p.Pt_Light, pj->p.Pt_Light, num, den);
            if (m < outCap) {
                out[m++] = v;
            }
        }
    }

    return m;
}

/* Clip a body polygon against the near plane and the safe screen edges in
   camera space, then project the result.  `cam` is three S32 per vertex
   (x, y, depth); `pts` carries the matching screen/UV/light attributes.
   Returns the vertex count written to `out` (0 when the polygon lies
   entirely outside). */
S32 GL_ClipPolyToNear(const S32 *cam, const Struc_Point *pts, S32 n,
                      S32 nearD, Struc_Point *out, S32 outCap) {
    T_CLIP_VERT a[12];
    T_CLIP_VERT b[12];
    T_CLIP_VERT *in = a;
    T_CLIP_VERT *work = b;
    S32 m = n;

    if (n < 0 || n > 4) {
        return 0;
    }
    for (S32 i = 0; i < n; i++) {
        in[i].x = cam[(i * 3) + 0];
        in[i].y = cam[(i * 3) + 1];
        in[i].z = cam[(i * 3) + 2];
        in[i].p = pts[i];
    }

    const S32 nearPlane = (nearD > 0) ? nearD : 1;
    const S64 fx = (S64)lrint((double)FRatioX);
    const S64 ky = (S64)lrint((double)FRatioX * (double)FRatioY);
    const S64 cx = (S64)XCentre;
    const S64 cy = (S64)YCentre;

    const S64 planes[5][4] = {
        {0, 0, 1, -(S64)nearPlane},          /* z >= near */
        {-fx, 0, CLIP_SCREEN_BOUND - cx, 0}, /* screen x <= +bound */
        {fx, 0, cx + CLIP_SCREEN_BOUND, 0},  /* screen x >= -bound */
        {0, -ky, CLIP_SCREEN_BOUND - cy, 0}, /* screen y <= +bound */
        {0, ky, cy + CLIP_SCREEN_BOUND, 0}   /* screen y >= -bound */
    };

    for (S32 p = 0; p < 5 && m >= 3; p++) {
        m = ClipAgainstPlane(in, m, planes[p][0], planes[p][1], planes[p][2],
                             planes[p][3], work, 12);
        T_CLIP_VERT *t = in;
        in = work;
        work = t;
    }
    if (m < 3) {
        return 0;
    }

    if (m > outCap) {
        m = outCap;
    }
    for (S32 i = 0; i < m; i++) {
        const S32 c[3] = {in[i].x, in[i].y, in[i].z};
        out[i] = in[i].p;
        GL_ProjectPolyCam(c, &out[i]);
    }
    return m;
}
