/* ── xBR-lv2 2x edge-directed upscale (CPU) ─────────────────────────────── */
/* Scalar port of Hyllian's xBR-lv2 shader (CORNER_C variant, rgbw luminance,
 * small-details path), fixed 2x scale.  Same edge rules as the per-texture
 * sampling in LIB386/GL/RENDER_GL.CPP's CLUT shader (texture filter mode 3);
 * this twin exists so host tests pin those rules.
 *
 *   xBR-lv2 Shader, Copyright (C) 2011-2016 Hyllian <sergiogdb@gmail.com>.
 *   Incorporates ideas from the SABR shader (Joshua Street).
 *   MIT license — full notice in the CLUT shader source.
 *
 * The four blend candidates (shader vec4 lanes) are kept as float[4] arrays
 * indexed like the shader: 0=NW, 1=NE, 2=SW, 3=SE. */

#include <SVGA/XBR.H>

#define XBR_EQ_THRESHOLD 15.0f
#define XBR_LV2_COEFFICIENT 2.0f

static const float XBR_RGBW[3] = {14.352f, 28.176f, 5.472f};
static const float XBR_DELTA[4] = {1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f,
                                   1.0f / 3.0f};
static const float XBR_DELTA_L[4] = {0.5f / 3.0f, 1.0f / 3.0f, 0.5f / 3.0f,
                                     1.0f / 3.0f};
static const float XBR_DELTA_U[4] = {1.0f / 3.0f, 0.5f / 3.0f, 1.0f / 3.0f,
                                     0.5f / 3.0f};
/* Line inequations separating the four output quadrants (shader Ao..Cy). */
static const float XBR_AO[4] = {1.0f, -1.0f, -1.0f, 1.0f};
static const float XBR_BO[4] = {1.0f, 1.0f, -1.0f, -1.0f};
static const float XBR_CO[4] = {1.5f, 0.5f, -0.5f, 0.5f};
static const float XBR_AX[4] = {1.0f, -1.0f, -1.0f, 1.0f};
static const float XBR_BX[4] = {0.5f, 2.0f, -0.5f, -2.0f};
static const float XBR_CX[4] = {1.0f, 1.0f, -0.5f, 0.0f};
static const float XBR_AY[4] = {1.0f, -1.0f, -1.0f, 1.0f};
static const float XBR_BY[4] = {2.0f, 0.5f, -2.0f, -0.5f};
static const float XBR_CY[4] = {2.0f, 0.0f, -1.0f, 0.5f};

#define XBR_CI 0.25f

/* Unpack an XRGB8888 word to floats (alpha is not part of the filter). */
static void XbrUnpack(U32 c, float *rgb) {
    rgb[0] = (float)((c >> 16) & 0xFF) / 255.0f;
    rgb[1] = (float)((c >> 8) & 0xFF) / 255.0f;
    rgb[2] = (float)(c & 0xFF) / 255.0f;
}

static U32 XbrPack(const float *rgb) {
    S32 r = (S32)(rgb[0] * 255.0f + 0.5f);
    S32 g = (S32)(rgb[1] * 255.0f + 0.5f);
    S32 b = (S32)(rgb[2] * 255.0f + 0.5f);
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;
    return 0xFF000000u | ((U32)r << 16) | ((U32)g << 8) | (U32)b;
}

/* CLAMP_TO_EDGE fetch, matching the GPU composite texture's wrap mode. */
static void XbrFetch(const U32 *src, U32 w, U32 h, S32 x, S32 y, float *rgb) {
    if (x < 0)
        x = 0;
    if ((U32)x >= w)
        x = (S32)w - 1;
    if (y < 0)
        y = 0;
    if ((U32)y >= h)
        y = (S32)h - 1;
    XbrUnpack(src[(U32)y * w + (U32)x], rgb);
}

/* Project a color onto the xBR luminance axis (shader dot(c, rgbw)). */
static float XbrLuma(const float *c) {
    return c[0] * XBR_RGBW[0] + c[1] * XBR_RGBW[1] + c[2] * XBR_RGBW[2];
}

/* Blend `to` into `base` by weight `t` (shader mix(base, to, t)). */
static void XbrMix(const float *base, const float *to, float t, float *out) {
    out[0] = base[0] + (to[0] - base[0]) * t;
    out[1] = base[1] + (to[1] - base[1]) * t;
    out[2] = base[2] + (to[2] - base[2]) * t;
}

static float XbrColorDist(const float *a, const float *b) {
    float dr = a[0] - b[0];
    float dg = a[1] - b[1];
    float db = a[2] - b[2];
    if (dr < 0)
        dr = -dr;
    if (dg < 0)
        dg = -dg;
    if (db < 0)
        db = -db;
    return dr + dg + db;
}

static float XbrAbs(float v) { return v < 0.0f ? -v : v; }

void XbrUpscale2x(const U32 *src, U32 srcW, U32 srcH, U32 *dst) {
    U32 dstW = srcW * 2;

    for (U32 oy = 0; oy < srcH * 2; oy++) {
        U32 y = oy >> 1;
        /* Output pixel centres sit at 1/4 and 3/4 inside the source texel —
         * the fp the shader computes from fract(uv * texSize) at 2x. */
        float fpy = (oy & 1) ? 0.75f : 0.25f;
        U32 *dstRow = dst + (U32)oy * dstW;

        for (U32 ox = 0; ox < dstW; ox++) {
            U32 x = ox >> 1;
            float fpx = (ox & 1) ? 0.75f : 0.25f;

            /*    A1 B1 C1                                  */
            /* A0  A  B  C C4                              */
            /* D0  D  E  F F4                              */
            /* G0  G  H  I I4                              */
            /*    G5 H5 I5                                 */
            float A1[3], B1[3], C1[3], A[3], B[3], C[3], D[3], E[3], F[3],
                G[3], H[3], I[3], G5[3], H5[3], I5[3], A0[3], D0[3], G0[3],
                C4[3], F4[3], I4[3];
            XbrFetch(src, srcW, srcH, (S32)x - 1, (S32)y - 2, A1);
            XbrFetch(src, srcW, srcH, (S32)x, (S32)y - 2, B1);
            XbrFetch(src, srcW, srcH, (S32)x + 1, (S32)y - 2, C1);
            XbrFetch(src, srcW, srcH, (S32)x - 1, (S32)y - 1, A);
            XbrFetch(src, srcW, srcH, (S32)x, (S32)y - 1, B);
            XbrFetch(src, srcW, srcH, (S32)x + 1, (S32)y - 1, C);
            XbrFetch(src, srcW, srcH, (S32)x - 1, (S32)y, D);
            XbrFetch(src, srcW, srcH, (S32)x, (S32)y, E);
            XbrFetch(src, srcW, srcH, (S32)x + 1, (S32)y, F);
            XbrFetch(src, srcW, srcH, (S32)x - 1, (S32)y + 1, G);
            XbrFetch(src, srcW, srcH, (S32)x, (S32)y + 1, H);
            XbrFetch(src, srcW, srcH, (S32)x + 1, (S32)y + 1, I);
            XbrFetch(src, srcW, srcH, (S32)x - 1, (S32)y + 2, G5);
            XbrFetch(src, srcW, srcH, (S32)x, (S32)y + 2, H5);
            XbrFetch(src, srcW, srcH, (S32)x + 1, (S32)y + 2, I5);
            XbrFetch(src, srcW, srcH, (S32)x - 2, (S32)y - 1, A0);
            XbrFetch(src, srcW, srcH, (S32)x - 2, (S32)y, D0);
            XbrFetch(src, srcW, srcH, (S32)x - 2, (S32)y + 1, G0);
            XbrFetch(src, srcW, srcH, (S32)x + 2, (S32)y - 1, C4);
            XbrFetch(src, srcW, srcH, (S32)x + 2, (S32)y, F4);
            XbrFetch(src, srcW, srcH, (S32)x + 2, (S32)y + 1, I4);

            /* Packed luminances (shader vec4 lanes NW/NE/SW/SE). */
            float b[4], c[4], d[4], e[4], f[4], g[4], h[4], i[4], i4[4],
                i5[4], h5[4], f4[4];
            b[0] = XbrLuma(B);
            b[1] = XbrLuma(D);
            b[2] = XbrLuma(H);
            b[3] = XbrLuma(F);
            c[0] = XbrLuma(C);
            c[1] = XbrLuma(A);
            c[2] = XbrLuma(G);
            c[3] = XbrLuma(I);
            d[0] = b[1];
            d[1] = b[2];
            d[2] = b[3];
            d[3] = b[0];
            e[0] = e[1] = e[2] = e[3] = XbrLuma(E);
            f[0] = b[3];
            f[1] = b[0];
            f[2] = b[1];
            f[3] = b[2];
            g[0] = c[2];
            g[1] = c[3];
            g[2] = c[0];
            g[3] = c[1];
            h[0] = b[2];
            h[1] = b[3];
            h[2] = b[0];
            h[3] = b[1];
            i[0] = c[3];
            i[1] = c[0];
            i[2] = c[1];
            i[3] = c[2];
            i4[0] = XbrLuma(I4);
            i4[1] = XbrLuma(C1);
            i4[2] = XbrLuma(A0);
            i4[3] = XbrLuma(G5);
            i5[0] = XbrLuma(I5);
            i5[1] = XbrLuma(C4);
            i5[2] = XbrLuma(A1);
            i5[3] = XbrLuma(G0);
            h5[0] = XbrLuma(H5);
            h5[1] = XbrLuma(F4);
            h5[2] = XbrLuma(B1);
            h5[3] = XbrLuma(D0);
            f4[0] = h5[1];
            f4[1] = h5[2];
            f4[2] = h5[3];
            f4[3] = h5[0];

            float fx[4], fxL[4], fxU[4];
            for (S32 l = 0; l < 4; l++) {
                fx[l] = XBR_AO[l] * fpy + XBR_BO[l] * fpx;
                fxL[l] = XBR_AX[l] * fpy + XBR_BX[l] * fpx;
                fxU[l] = XBR_AY[l] * fpy + XBR_BY[l] * fpx;
            }

            float fx45i[4], fx45[4], fx30[4], fx60[4];
            for (S32 l = 0; l < 4; l++) {
                fx45i[l] = (fx[l] + XBR_DELTA[l] - XBR_CO[l] - XBR_CI) /
                           (2.0f * XBR_DELTA[l]);
                fx45[l] = (fx[l] + XBR_DELTA[l] - XBR_CO[l]) /
                          (2.0f * XBR_DELTA[l]);
                fx30[l] = (fxL[l] + XBR_DELTA_L[l] - XBR_CX[l]) /
                          (2.0f * XBR_DELTA_L[l]);
                fx60[l] = (fxU[l] + XBR_DELTA_U[l] - XBR_CY[l]) /
                          (2.0f * XBR_DELTA_U[l]);
                if (fx45i[l] < 0)
                    fx45i[l] = 0;
                if (fx45i[l] > 1)
                    fx45i[l] = 1;
                if (fx45[l] < 0)
                    fx45[l] = 0;
                if (fx45[l] > 1)
                    fx45[l] = 1;
                if (fx30[l] < 0)
                    fx30[l] = 0;
                if (fx30[l] > 1)
                    fx30[l] = 1;
                if (fx60[l] < 0)
                    fx60[l] = 0;
                if (fx60[l] > 1)
                    fx60[l] = 1;
            }

            float wd1[4], wd2[4];
            float edri[4], edr[4], edrL[4], edrU[4], irlv0[4], irlv1[4],
                irlv2l[4], irlv2u[4], px[4];
            for (S32 l = 0; l < 4; l++) {
                /* irlv0: (e!=f) && (e!=h), exact compares like the shader. */
                irlv0[l] = (e[l] != f[l] && e[l] != h[l]) ? 1.0f : 0.0f;

                /* CORNER_C refinement. */
                float neqfb = (f[l] != b[l]) ? 1.0f : 0.0f;
                float neqfc = (f[l] != c[l]) ? 1.0f : 0.0f;
                float neqhd = (h[l] != d[l]) ? 1.0f : 0.0f;
                float neqhg = (h[l] != g[l]) ? 1.0f : 0.0f;
                float eqei = (XbrAbs(e[l] - i[l]) <= XBR_EQ_THRESHOLD) ? 1.0f
                                                                       : 0.0f;
                float neqff4 = (f[l] != f4[l]) ? 1.0f : 0.0f;
                float neqfi4 = (f[l] != i4[l]) ? 1.0f : 0.0f;
                float neqhh5 = (h[l] != h5[l]) ? 1.0f : 0.0f;
                float neqhi5 = (h[l] != i5[l]) ? 1.0f : 0.0f;
                float eqeg = (XbrAbs(e[l] - g[l]) <= XBR_EQ_THRESHOLD) ? 1.0f
                                                                       : 0.0f;
                float eqec = (XbrAbs(e[l] - c[l]) <= XBR_EQ_THRESHOLD) ? 1.0f
                                                                       : 0.0f;
                irlv1[l] = irlv0[l] * (neqfb * neqfc + neqhd * neqhg +
                                       eqei * (neqff4 * neqfi4 +
                                               neqhh5 * neqhi5) +
                                       eqeg + eqec);

                irlv2l[l] = (e[l] != g[l] && d[l] != g[l]) ? 1.0f : 0.0f;
                irlv2u[l] = (e[l] != c[l] && b[l] != c[l]) ? 1.0f : 0.0f;

                /* Weighted diagonal distances (shader wd()). */
                wd1[l] = XbrAbs(e[l] - c[l]) + XbrAbs(e[l] - g[l]) +
                         XbrAbs(i[l] - h5[l]) + XbrAbs(i[l] - f4[l]) +
                         4.0f * XbrAbs(h[l] - f[l]);
                wd2[l] = XbrAbs(h[l] - d[l]) + XbrAbs(h[l] - i5[l]) +
                         XbrAbs(f[l] - i4[l]) + XbrAbs(f[l] - b[l]) +
                         4.0f * XbrAbs(e[l] - i[l]);

                edri[l] = (wd1[l] <= wd2[l]) ? 1.0f : 0.0f;
                edri[l] *= irlv0[l];
                edr[l] = (wd1[l] + 0.1f <= wd2[l] && irlv1[l] >= 0.5f) ? 1.0f
                                                                       : 0.0f;

                float dfg = XbrAbs(f[l] - g[l]);
                float dhc = XbrAbs(h[l] - c[l]);
                edrL[l] = (XBR_LV2_COEFFICIENT * dfg <= dhc) ? 1.0f : 0.0f;
                edrL[l] *= irlv2l[l] * edr[l];
                edrU[l] = (dfg >= XBR_LV2_COEFFICIENT * dhc) ? 1.0f : 0.0f;
                edrU[l] *= irlv2u[l] * edr[l];

                px[l] = (XbrAbs(e[l] - f[l]) <= XbrAbs(e[l] - h[l])) ? 1.0f
                                                                     : 0.0f;
            }

            float maximos[4];
            for (S32 l = 0; l < 4; l++) {
                fx45[l] *= edr[l];
                fx30[l] *= edrL[l];
                fx60[l] *= edrU[l];
                fx45i[l] *= edri[l];
                float m1 = fx30[l] > fx60[l] ? fx30[l] : fx60[l];
                float m2 = fx45[l] > fx45i[l] ? fx45[l] : fx45i[l];
                maximos[l] = m1 > m2 ? m1 : m2;
            }

            float res1[3], res2[3], tmp[3];
            float hf[3], bd[3], fb[3], dh[3];
            for (S32 k = 0; k < 3; k++) {
                hf[k] = H[k] + (F[k] - H[k]) * px[0];
                bd[k] = B[k] + (D[k] - B[k]) * px[2];
                fb[k] = F[k] + (B[k] - F[k]) * px[1];
                dh[k] = D[k] + (H[k] - D[k]) * px[3];
            }
            XbrMix(E, hf, maximos[0], tmp);
            res1[0] = tmp[0];
            res1[1] = tmp[1];
            res1[2] = tmp[2];
            XbrMix(res1, bd, maximos[2], res1);
            XbrMix(E, fb, maximos[1], tmp);
            res2[0] = tmp[0];
            res2[1] = tmp[1];
            res2[2] = tmp[2];
            XbrMix(res2, dh, maximos[3], res2);

            /* Keep the candidate closer to E (step on the color distance). */
            float *res =
                (XbrColorDist(E, res1) <= XbrColorDist(E, res2)) ? res1 : res2;

            dstRow[ox] = XbrPack(res);
        }
    }
}
