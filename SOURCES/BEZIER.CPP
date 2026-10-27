/*══════════════════════════════════════════════════════════════════════════*

		██████┐ █┐   █┐ █████┐  ██████┐ ██████┐ ██████┐
		██┌─██│ ██┐ ██│ ██┌██│  ██┌─██│ ██┌───┘ ██┌───┘
		██│ ██│ ██████│ ██████┐ ██████│ █████┐  ██████┐
		██│ ██│ ██┌─██│ ██┌─██│ ██┌██┌┘ ██┌──┘  └───██│
		██████│ ██│ ██│ ██████│ ██│└██┐ ██████┐ ██████│
		└─────┘ └─┘ └─┘ └─────┘ └─┘ └─┘ └─────┘ └─────┘

			   (merci Time Commando !!!)

 *══════════════════════════════════════════════════════════════════════════*/

#include "C_EXTERN.H"

#ifdef USE_GPURENDERER
#include <3D/PROJ.H>
#include <GL/RENDER_GL.H> /* GL_DrawShadowPoly for DrawShadowGL */
#endif

// One entry per screen scanline: sizes the shadow-fill span tables below and
// bounds the fill loop. Must cover the active ModeResY, so it tracks the
// max supported height (ADELINE_MAX_Y_RES) exactly like TabOffLine[]. Pinning
// it to the native 480 culled every actor shadow below line 480 at above-native
// vertical resolutions (720p dropped 240 lines, 1080p dropped 600).
#define MAX_SCAN_LINES ADELINE_MAX_Y_RES

/*──────────────────────────────────────────────────────────────────────────*/
//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
S32 TabCoorPoint[MAX_SCAN_LINES][4];
S32 TabNbPoint[MAX_SCAN_LINES];
S32 MinYTabPoint, MaxYTabPoint;

S32 XS0, YS0, XS1, YS1, XS2, YS2, XS3, YS3;
U16 ShadowZO[4]; /* camera-space depth per projected corner (GL shadow quad) */

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
void CalculeLigneOmbre(S32 x1, S32 y1, S32 x2, S32 y2) {
    S32 temp;
    S32 ix;
    S32 x, y;
    S32 hauteur;

    if (y1 > y2) {
        temp = x1;
        x1 = x2;
        x2 = temp;

        temp = y1;
        y1 = y2;
        y2 = temp;
    }

    if ((y2 >= ClipYMin) AND(y1 <= ClipYMax)) {
        hauteur = y2 - y1 + 1;

        ix = WTOF(x2 - x1) / hauteur;

        x = WTOF(x1);

        if (y1 < ClipYMin) {
            x += ix * (ClipYMin - y1 - 1);
            y1 = ClipYMin - 1;
        }

        if (y2 > ClipYMax)
            y2 = ClipYMax;
        x += ix;
        y1++;

        if (y1 < MinYTabPoint)
            MinYTabPoint = y1;
        if (y2 > MaxYTabPoint)
            MaxYTabPoint = y2;

        for (y = y1; y <= y2; y++) {
            if ((U32)y < MAX_SCAN_LINES AND(U32) TabNbPoint[y] < 4) {
                TabCoorPoint[y][TabNbPoint[y]++] = FTOW(x);
            }
            //Plot(FTOW(x),y,15);
            x += ix;
        }
    }
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
/* Walk the Bezier-rounded outline of the projected shadow quad: from the
   midpoint of edge 0-1, each edge becomes a quadratic curve toward its
   corner (three levels of midpoint cutting), which rounds the diamond
   formed by the four projected bounding-box edge midpoints.  Emits the
   exact endpoint sequence the span filler traces segment by segment:
   entry 0 is the pixel-space start point, the rest the FTOW'd outline
   points.  oz mirrors the same midpoint arithmetic over the corner camera
   depths (ShadowZO) so a depth-tested GPU fill can interpolate ZO per
   vertex.  Returns the number of emitted points. */
static S32 BuildOmbreOutline(S32 x0, S32 y0, S32 x1, S32 y1, S32 x2, S32 y2,
                             S32 x3, S32 y3, const U16 *zcorn, S32 *ox,
                             S32 *oy, S32 *oz, S32 maxPts) {
    S32 x[4];
    S32 y[4];
    S32 z[4];
    S32 firstx, firsty, firstz;
    S32 idx = 0;
    S32 n, i;

    x[0] = WTOF(x0);
    y[0] = WTOF(y0);
    x[1] = WTOF(x1);
    y[1] = WTOF(y1);
    x[2] = WTOF(x2);
    y[2] = WTOF(y2);
    x[3] = WTOF(x3);
    y[3] = WTOF(y3);

    for (i = 0; i < 4; i++) {
        z[i] = zcorn[i];
    }

    firstx = (x[0] + x[1]) >> 1;
    firsty = (y[0] + y[1]) >> 1;
    firstz = (z[0] + z[1]) >> 1;

    if (idx < maxPts) {
        ox[idx] = (x0 + x1) >> 1;
        oy[idx] = (y0 + y1) >> 1;
        oz[idx] = firstz;
        idx++;
    }

    for (n = 0; n < 4; n++) {
        S32 ax = x[(n + 1) & 3];
        S32 lastx = (ax + x[(n + 2) & 3]) >> 1;
        S32 ay = y[(n + 1) & 3];
        S32 lasty = (ay + y[(n + 2) & 3]) >> 1;
        S32 ac = z[(n + 1) & 3];
        S32 lastz = (ac + z[(n + 2) & 3]) >> 1;

        S32 b1x = (firstx + ax) >> 1;
        S32 b2x = (lastx + ax) >> 1;
        S32 midx = (b1x + b2x) >> 1;
        S32 b1y = (firsty + ay) >> 1;
        S32 b2y = (lasty + ay) >> 1;
        S32 midy = (b1y + b2y) >> 1;
        S32 b1z = (firstz + ac) >> 1;
        S32 b2z = (lastz + ac) >> 1;
        S32 midz = (b1z + b2z) >> 1;

        S32 c1x = (b1x + firstx) >> 1;
        S32 c2x = (b1x + midx) >> 1;
        S32 inter1x = (c1x + c2x) >> 1;
        S32 c3x = (b2x + midx) >> 1;
        S32 c4x = (b2x + lastx) >> 1;
        S32 inter2x = (c3x + c4x) >> 1;

        S32 c1y = (b1y + firsty) >> 1;
        S32 c2y = (b1y + midy) >> 1;
        S32 inter1y = (c1y + c2y) >> 1;
        S32 c3y = (b2y + midy) >> 1;
        S32 c4y = (b2y + lasty) >> 1;
        S32 inter2y = (c3y + c4y) >> 1;

        S32 c1z = (b1z + firstz) >> 1;
        S32 c2z = (b1z + midz) >> 1;
        S32 inter1z = (c1z + c2z) >> 1;
        S32 c3z = (b2z + midz) >> 1;
        S32 c4z = (b2z + lastz) >> 1;
        S32 inter2z = (c3z + c4z) >> 1;

        S32 px[4];
        S32 py[4];
        S32 pz[4];

        px[0] = inter1x;
        py[0] = inter1y;
        pz[0] = inter1z;
        px[1] = midx;
        py[1] = midy;
        pz[1] = midz;
        px[2] = inter2x;
        py[2] = inter2y;
        pz[2] = inter2z;
        px[3] = lastx;
        py[3] = lasty;
        pz[3] = lastz;

        for (i = 0; i < 4; i++) {
            if (idx < maxPts) {
                ox[idx] = FTOW(px[i]);
                oy[idx] = FTOW(py[i]);
                oz[idx] = pz[i];
                idx++;
            }
        }

        firstx = lastx;
        firsty = lasty;
        firstz = lastz;
    }

    return idx;
}

void DrawOmbre(S32 x0, S32 y0, S32 x1, S32 y1, S32 x2, S32 y2, S32 x3, S32 y3) {
    S32 px[17];
    S32 py[17];
    S32 pz[17];
    S32 start_x, start_y;
    S32 nb, i;

    nb = BuildOmbreOutline(x0, y0, x1, y1, x2, y2, x3, y3, ShadowZO, px, py,
                           pz, 17);

    start_x = px[0];
    start_y = py[0];

    for (i = 1; i < nb; i++) {
        CalculeLigneOmbre(start_x, start_y, px[i], py[i]);
        start_x = px[i];
        start_y = py[i];
    }
}

#ifdef USE_GPURENDERER
/* GL mode: fill the same Bezier-rounded outline the span filler traces as
   one multiply-blended, depth-tested polygon straight into the FBO —
   occlusion comes from the FBO depth instead of DrawRecover fixing up
   Log.  The last outline point re-closes the loop at the start point
   (both are the midpoint of edge 0-1, one in pixel space, one in fixed
   point), so the polygon drops it. */
void DrawShadowGL(void) {
    S32 px[17];
    S32 py[17];
    S32 pz[17];
    S16 sx[16];
    S16 sy[16];
    U16 sz[16];
    S32 nb, i;

    nb = BuildOmbreOutline(XS0, YS0, XS1, YS1, XS2, YS2, XS3, YS3, ShadowZO,
                           px, py, pz, 17);

    if (nb > 16) {
        nb = 16;
    }

    /* GL vertices are S16; a projection blowup wraps them into arbitrary
       on-screen triangles. */
    for (i = 0; i < nb; i++) {
        if (px[i] < -32767 || px[i] > 32767 || py[i] < -32767 ||
            py[i] > 32767) {
            return;
        }
        sx[i] = (S16)px[i];
        sy[i] = (S16)py[i];
        /* The depth is already on the scene scale: ProjectShadow maps interior
           corners through IsoDepthToAz, and the exterior keeps GET_ZO. */
        sz[i] = (U16)pz[i];
    }

    if (nb >= 3) {
        /* Only the exterior has a terrain depth for the fill to fight. */
        GL_DrawShadowPoly(sx, sy, sz, nb, ShadowLevel,
                          CubeMode != CUBE_INTERIEUR);
    }
}
#endif

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
void DrawLineShade(S32 x0, S32 z0, S32 x1, S32 z1, S32 x2, S32 z2, S32 x3, S32 z3) {
    S32 y, j, k;
    S32 temp;
    S32 yStart, yEnd;

    MinYTabPoint = ClipYMax;
    MaxYTabPoint = ClipYMin;
    DrawOmbre(x0, z0, x1, z1, x2, z2, x3, z3);

    yStart = MinYTabPoint;
    yEnd = MaxYTabPoint;

    if (yStart < 0)
        yStart = 0;
    if (yEnd > MAX_SCAN_LINES - 1)
        yEnd = MAX_SCAN_LINES - 1;

    for (y = yStart; y <= yEnd; y++) {
        switch (TabNbPoint[y]) {
        case 2: //----------------------------------------------------

            if (TabCoorPoint[y][0] < TabCoorPoint[y][1])
                ShadeBoxBlk(TabCoorPoint[y][0], y, TabCoorPoint[y][1], y, ShadowLevel);
            else
                ShadeBoxBlk(TabCoorPoint[y][1], y, TabCoorPoint[y][0], y, ShadowLevel);
            break;

        case 4: //----------------------------------------------------
            for (j = 0; j <= 2; j++) {
                for (k = 1; k <= 3; k++) {
                    if (TabCoorPoint[y][k] < TabCoorPoint[y][j]) {
                        temp = TabCoorPoint[y][j];
                        TabCoorPoint[y][j] = TabCoorPoint[y][k];
                        TabCoorPoint[y][k] = temp;
                    }
                }
            }

            if (TabCoorPoint[y][0] < TabCoorPoint[y][1])
                ShadeBoxBlk(TabCoorPoint[y][0], y, TabCoorPoint[y][1], y, ShadowLevel);
            else
                ShadeBoxBlk(TabCoorPoint[y][1], y, TabCoorPoint[y][0], y, ShadowLevel);

            if (TabCoorPoint[y][2] < TabCoorPoint[y][3])
                ShadeBoxBlk(TabCoorPoint[y][2], y, TabCoorPoint[y][3], y, ShadowLevel);
            else
                ShadeBoxBlk(TabCoorPoint[y][3], y, TabCoorPoint[y][2], y, ShadowLevel);
            break;
        }

        TabNbPoint[y] = 0; // reinit for next Shadow
    }
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
// Version ZVs absolues précalculées
S32 TestZVDecorsXZFull(S32 xw, S32 zw, S32 y0, S32 y1, T_DECORS *objet) {
    if (objet->Body & DEC_INVISIBLE)
        return FALSE;

    if (objet->XMin > xw)
        return FALSE;
    if (objet->XMax < xw)
        return FALSE;

    if (objet->ZMin > zw)
        return FALSE;
    if (objet->ZMax < zw)
        return FALSE;

    if (objet->YMin > y1)
        return FALSE;
    if (objet->YMax < y0)
        return FALSE;

    return TRUE;
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
S32 TestZVObjectsXZFull(S32 xw, S32 zw, S32 y0, S32 y1, T_OBJET *objet) {
    S32 ymin, ymax;

    ymin = objet->Obj.Y + objet->YMin;
    ymax = objet->Obj.Y + objet->YMax;

    if (ymin<y0 OR ymin> /*=*/y1 OR ymax > y1)
        return FALSE;
    if (ymax < y0)
        return FALSE;

    if (objet->Obj.Body.Num == -1 OR objet->Flags & INVISIBLE)
        return FALSE;

    if ((objet->Obj.X + objet->XMin) > xw)
        return FALSE;
    if ((objet->Obj.X + objet->XMax) < xw)
        return FALSE;

    if ((objet->Obj.Z + objet->ZMin) > zw)
        return FALSE;
    if ((objet->Obj.Z + objet->ZMax) < zw)
        return FALSE;

    return TRUE;
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
S32 ShadowTestDecors = 0;

S32 AdjustShadowDecors(S32 xw, S32 zw, S32 y0, S32 y1) {
    S32 i;
    T_DECORS *ptrd;
    T_LIST_TRI *ptrtri = ListTriExt;

    // decors
    for (i = 0; i < NbObjScreen; i++, ptrtri++) {
        ptrd = ptrtri->PtrDec;

        if (ptrd->Body & DEC_DRAWN OR FirstLoop) {
            if (TestZVDecorsXZFull(xw, zw, y0, y1, ptrd)) {
                y0 = ptrd->YMax;
            }
        }
    }

    return y0;
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
S32 ShadowTestObject = 0;

S32 AdjustShadowObjects(S32 xw, S32 zw, S32 y0, S32 y1) {
    S32 i;
    T_OBJET *ptrobj = ListObjet;

    ShadowTestObject = -1;

    // decors
    for (i = 0; i < NbObjets; i++, ptrobj++) {
        if (ptrobj->WorkFlags & WAS_DRAWN OR FirstLoop) {
            if (TestZVObjectsXZFull(xw, zw, y0, y1, ptrobj)) {
                y0 = ptrobj->Obj.Y + ptrobj->YMax;
                ShadowTestObject = i;
            }
        }
    }

    return y0;
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
S32 ProjectShadowExt(T_SORT *ptrs, S32 beta) {
    S32 xmin, zmin, xmax, zmax, xw, yw, zw;
    S32 cy;
    S32 altitude_ref;
    S32 dx2, dz2;

    ClearScreenMinMax();

    xw = ptrs->Xw;
    yw = ptrs->Yw;
    zw = ptrs->Zw;

    xmin = ptrs->XMin - xw;
    zmin = ptrs->ZMin - zw;
    xmax = ptrs->XMax - xw;
    zmax = ptrs->ZMax - zw;

    dx2 = (xmin + xmax) / 2;
    dz2 = (zmin + zmax) / 2;

    // inclure ici la gestion des objets du decors
    altitude_ref = yw;
    //	altitude_ref = CalculAltitudeObjet( xw, zw );
    altitude_ref = AdjustShadowDecors(xw, zw, altitude_ref, ptrs->YMax /*yw*/);
    altitude_ref = AdjustShadowObjects(xw, zw, altitude_ref, ptrs->YMax /*yw*/);

    ptrs->Yw = altitude_ref; // Pour les recouvrements
    ptrs->YMax = altitude_ref + (ptrs->YMax - yw);

    Rotate(dx2, zmin, beta);

    X0 += xw;
    Z0 += zw;

    cy = CalculAltitudeObjet(X0, Z0, -1);
    cy = AdjustShadowDecors(X0, Z0, cy, altitude_ref);
    cy = AdjustShadowObjects(X0, Z0, cy, altitude_ref);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    LongWorldRotatePoint(X0, cy, Z0);
    ShadowZO[0] = GET_ZO(CameraZr - Z0);
    if (!LongProjectPoint(X0, Y0, Z0))
        return FALSE;
    XS0 = Xp;
    YS0 = Yp;
    AdjustScreenMinMax();

    Rotate(xmin, dz2, beta);

    X0 += xw;
    Z0 += zw;

    cy = CalculAltitudeObjet(X0, Z0, -1);
    cy = AdjustShadowDecors(X0, Z0, cy, altitude_ref);
    cy = AdjustShadowObjects(X0, Z0, cy, altitude_ref);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    LongWorldRotatePoint(X0, cy, Z0);
    ShadowZO[1] = GET_ZO(CameraZr - Z0);
    if (!LongProjectPoint(X0, Y0, Z0))
        return FALSE;
    XS1 = Xp;
    YS1 = Yp;
    AdjustScreenMinMax();

    Rotate(dx2, zmax, beta);

    X0 += xw;
    Z0 += zw;

    cy = CalculAltitudeObjet(X0, Z0, -1);
    cy = AdjustShadowDecors(X0, Z0, cy, altitude_ref);
    cy = AdjustShadowObjects(X0, Z0, cy, altitude_ref);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    LongWorldRotatePoint(X0, cy, Z0);
    ShadowZO[2] = GET_ZO(CameraZr - Z0);
    if (!LongProjectPoint(X0, Y0, Z0))
        return FALSE;
    XS2 = Xp;
    YS2 = Yp;
    AdjustScreenMinMax();

    Rotate(xmax, dz2, beta);

    X0 += xw;
    Z0 += zw;

    cy = CalculAltitudeObjet(X0, Z0, -1);
    cy = AdjustShadowDecors(X0, Z0, cy, altitude_ref);
    cy = AdjustShadowObjects(X0, Z0, cy, altitude_ref);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    LongWorldRotatePoint(X0, cy, Z0);
    ShadowZO[3] = GET_ZO(CameraZr - Z0);
    if (!LongProjectPoint(X0, Y0, Z0))
        return FALSE;
    XS3 = Xp;
    YS3 = Yp;
    AdjustScreenMinMax();

    //	UnsetClip() ;
    SetClip(ScreenXMin, ScreenYMin, ScreenXMax, ScreenYMax);

    return TRUE;
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
void DrawShadow() {
    DrawLineShade(XS0, YS0, XS1, YS1, XS2, YS2, XS3, YS3);

    //	BoxMovingAdd( ClipXMin, ClipYMin, ClipXMax, ClipYMax ) ;

#ifdef LBA_EDITOR
    if (FlagInfos & INFO_DRAW_ZV) {
        Line(XS0, YS0, XS1, YS1, BLUE);
        Line(XS1, YS1, XS2, YS2, BLUE);
        Line(XS2, YS2, XS3, YS3, BLUE);
        Line(XS3, YS3, XS0, YS0, BLUE);
    }
#endif
}
