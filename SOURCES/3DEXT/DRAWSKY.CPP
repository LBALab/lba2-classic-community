
/* Cheap sea/sky quad reject.  A quad that is wholly outside the frame
   cannot contribute a pixel; its four rotations/projections and filler call
   are pure sea-view CPU cost.  The bound is a circumscribed sphere around
   the quad: the far test uses the sphere's nearest depth, and the screen
   test scales the radius by the centre's off-axis offset, so a quad that
   is still partly visible is never rejected.  A failed projection or a
   near quad is kept and left to Draw_Poly's exact clipper. */
static S32 SkySeaQuadVisible(S32 x0, S32 z0, S32 y, S32 halfWorld) {
    static S32 s_noSeaCull = -1;
    if (s_noSeaCull < 0) {
        s_noSeaCull = getenv("LBA2_NO_SEA_CULL") ? 1 : 0;
    }
    if (s_noSeaCull) {
        return 1;
    }

    LongWorldRotatePoint(x0 + halfWorld, y, z0 + halfWorld);
    if (!LongProjectPoint(X0, Y0, Z0)) {
        return 1;
    }
    const S32 depth = CameraZr - Z0;
    /* The square's circumradius is halfWorld*sqrt(2); 2*halfWorld is a
       cheap over-estimate that keeps the depth and screen bounds safe. */
    const S32 radius = halfWorld * 2;
    const S32 nearest = depth - radius;
    if (depth < NearClip || nearest < NearClip) {
        return 1;
    }
    if (nearest > ClipZFar) {
        return 0;
    }
    const S32 ax = X0 < 0 ? -X0 : X0;
    const S32 ay = Y0 < 0 ? -Y0 : Y0;
    const double denom = (double)depth * (double)nearest;
    const double halfX = (double)radius * (double)FRatioX *
                             ((double)depth + (double)ax) / denom +
                         3.0;
    const double halfY = (double)radius * (double)FRatioY *
                             ((double)depth + (double)ay) / denom +
                         3.0;
    if ((double)Xp + halfX < 0.0 ||
        (double)Xp - halfX > (double)ModeDesiredX) {
        return 0;
    }
    if ((double)Yp + halfY < 0.0 ||
        (double)Yp - halfY > (double)ModeDesiredY) {
        return 0;
    }
    return 1;
}

typedef struct {
    S32 X, Y, Z;
    U16 U, V, Light;
} T_SEA_VERT;

/* Sutherland-Hodgman against one screen edge, interpolating the projected
   X/Y, camera Z, UV and light.  Used only for sea/sky GL polygons whose
   projected corners can run far outside S16. */
static S32 SeaClipEdge(T_SEA_VERT *out, const T_SEA_VERT *in, S32 n,
                       S32 edge) {
    S32 m = 0;
    for (S32 i = 0; i < n; i++) {
        const T_SEA_VERT *a = &in[i];
        const T_SEA_VERT *b = &in[(i + 1) % n];
        S32 da, db;
        switch (edge) {
        case 0:
            da = a->X;
            db = b->X;
            break;
        case 1:
            da = (S32)ModeDesiredX - a->X;
            db = (S32)ModeDesiredX - b->X;
            break;
        case 2:
            da = a->Y;
            db = b->Y;
            break;
        default:
            da = (S32)ModeDesiredY - a->Y;
            db = (S32)ModeDesiredY - b->Y;
            break;
        }
        const S32 ina = da >= 0;
        const S32 inb = db >= 0;
        if (ina) {
            out[m++] = *a;
        }
        if (ina != inb) {
            const double t = (double)da / (double)(da - db);
            T_SEA_VERT *o = &out[m++];
            o->X = a->X + (S32)((double)(b->X - a->X) * t);
            o->Y = a->Y + (S32)((double)(b->Y - a->Y) * t);
            o->Light =
                (U16)(a->Light + (S32)(((S32)b->Light - (S32)a->Light) * t));
            if (a->Z > 0 && b->Z > 0) {
                /* The GL shader interpolates MapUV/Z and 1/Z in screen
                   space, then divides.  Clip in the same space: a plain
                   Z/UV lerp would put wrong values on the new screen-edge
                   vertex and shift the sea texture. */
                const double wa = 1.0 / (double)a->Z;
                const double wb = 1.0 / (double)b->Z;
                const double w = wa + (wb - wa) * t;
                const double uOverW =
                    (double)a->U * wa +
                    ((double)b->U * wb - (double)a->U * wa) * t;
                const double vOverW =
                    (double)a->V * wa +
                    ((double)b->V * wb - (double)a->V * wa) * t;
                o->Z = (S32)(1.0 / w + 0.5);
                o->U = (U16)(uOverW / w + 0.5);
                o->V = (U16)(vOverW / w + 0.5);
            } else {
                o->Z = a->Z + (S32)((double)(b->Z - a->Z) * t);
                o->U = (U16)(a->U + (S32)(((S32)b->U - (S32)a->U) * t));
                o->V = (U16)(a->V + (S32)(((S32)b->V - (S32)a->V) * t));
            }
        }
    }
    return m;
}

S32 Draw_Poly(STRUC_CLIPVERTEX src[], S32 type) {
    S32 k, l;
    STRUC_CLIPVERTEX dst[10];
    STRUC_CLIPVERTEX dst1[10];
    T_SEA_VERT sea[12];
#ifdef USE_GPURENDERER
    static S32 s_noSeaClamp = -1;
    if (s_noSeaClamp < 0) {
        s_noSeaClamp = getenv("LBA2_NO_SEA_CLAMP") ? 1 : 0;
    }
#endif

    k = ClipperZ(dst1, src, 4, NearClip + 1, 0);

    if (k) {
        k = ClipperZ(dst, dst1, k, Fill_Z_Fog_Far, -1);
        if (k) {
            for (l = 0; l != k; l++) {
                if (!LongProjectPoint(dst[l].V_X0 + CameraXr, dst[l].V_Y0 + CameraYr, CameraZr - dst[l].V_Z0))
                    return FALSE;
                sea[l].X = Xp;
                sea[l].Y = Yp;
                sea[l].Z = dst[l].V_Z0;
                sea[l].U = dst[l].V_MapU;
                sea[l].V = dst[l].V_MapV;
                sea[l].Light = dst[l].V_Light;
            }

#ifdef USE_GPURENDERER
            if (GPURendererActive && !s_noSeaClamp) {
                /* A large sea/sky tile can project far outside S16.  Clip
                   it against the real screen edges so the visible sheet
                   keeps the exact texture/light/Z interpolation; clamping
                   the corners instead shifted the sea/sky texture as the
                   camera moved. */
                T_SEA_VERT a[12], b[12];
                S32 n = k;
                for (S32 i = 0; i < n; i++) {
                    a[i] = sea[i];
                }
                n = SeaClipEdge(b, a, n, 0);
                n = SeaClipEdge(a, b, n, 1);
                n = SeaClipEdge(b, a, n, 2);
                n = SeaClipEdge(a, b, n, 3);
                if (n < 3) {
                    return FALSE;
                }
                for (S32 i = 0; i < n; i++) {
                    Tab_Points[i].Pt_XE = (S16)a[i].X;
                    Tab_Points[i].Pt_YE = (S16)a[i].Y;
                    Tab_Points[i].Pt_Light = a[i].Light;
                    Tab_Points[i].Pt_MapU = a[i].U;
                    Tab_Points[i].Pt_MapV = a[i].V;
                    Tab_Points[i].Pt_W = INV64(a[i].Z);
                    Tab_Points[i].Pt_ZO = GET_ZO(a[i].Z);
                }
                if (Fill_Poly(type, 0, n, Tab_Points))
                    return TRUE;
                return FALSE;
            }
#endif

            for (l = 0; l != k; l++) {
                if ((sea[l].X != (S32)(S16)sea[l].X) OR(sea[l].Y != (S32)(S16)sea[l].Y))
                    return FALSE;
                Tab_Points[l].Pt_XE = (S16)sea[l].X;
                Tab_Points[l].Pt_YE = (S16)sea[l].Y;
                Tab_Points[l].Pt_Light = sea[l].Light;
                Tab_Points[l].Pt_MapU = sea[l].U;
                Tab_Points[l].Pt_MapV = sea[l].V;
                Tab_Points[l].Pt_W = INV64(sea[l].Z);
                Tab_Points[l].Pt_ZO = GET_ZO(sea[l].Z);
            }

            if (Fill_Poly(type, 0, k, Tab_Points))
                return TRUE;
        }
    }
    return FALSE;
}

/*──────────────────────────────────────────────────────────────────────────*/

void DrawSky(S32 y) {
    U32 xx, zz, i, j;
    STRUC_CLIPVERTEX src[10];

    /* The sky is a flat sheet at Sky_Y.  From above it the software
       rasterizer rejects the tiles, but the GL path would draw the sheet
       over the view; skip it in both modes when the camera is not below. */
    if (CameraY >= y) {
        return;
    }

    PtrMap = SkySeaTexture + 128;
    RepMask = 0x7F7F;
    PtrCLUTGouraud = PtrCLUTFog;
    //	SetCLUT( PalLevel ) ;

    src[0].V_MapU = 0;
    src[0].V_MapV = 0;
    src[1].V_MapU = 0;
    src[1].V_MapV = 127 * 256 + 255;
    src[2].V_MapU = 127 * 256 + 255;
    src[2].V_MapV = 127 * 256 + 255;
    src[3].V_MapU = 127 * 256 + 255;
    src[3].V_MapV = 0;

    zz = -WORLD_SIZE;
    for (i = 0; i < NB_POLYS_COTE * 3; i++, zz += WORLD_SIZE / NB_POLYS_COTE) {
        xx = -WORLD_SIZE;
        for (j = 0; j < NB_POLYS_COTE * 3; j++, xx += WORLD_SIZE / NB_POLYS_COTE) {
            if (!SkySeaQuadVisible(xx, zz, y,
                                   WORLD_SIZE / (NB_POLYS_COTE * 2))) {
                continue;
            }
            LongWorldRotatePoint(xx, y, zz);
            src[0].V_X0 = X0 - CameraXr;
            src[0].V_Y0 = Y0 - CameraYr;
            src[0].V_Z0 = CameraZr - Z0;

            LongWorldRotatePoint(xx + WORLD_SIZE / NB_POLYS_COTE, y, zz);
            src[1].V_X0 = X0 - CameraXr;
            src[1].V_Y0 = Y0 - CameraYr;
            src[1].V_Z0 = CameraZr - Z0;

            LongWorldRotatePoint(xx + WORLD_SIZE / NB_POLYS_COTE, y, zz + WORLD_SIZE / NB_POLYS_COTE);
            src[2].V_X0 = X0 - CameraXr;
            src[2].V_Y0 = Y0 - CameraYr;
            src[2].V_Z0 = CameraZr - Z0;

            LongWorldRotatePoint(xx, y, zz + WORLD_SIZE / NB_POLYS_COTE);
            src[3].V_X0 = X0 - CameraXr;
            src[3].V_Y0 = Y0 - CameraYr;
            src[3].V_Z0 = CameraZr - Z0;

            Draw_Poly(&src[0], 24);
        }
    }
}

void DrawOneSea(S32 y) // Si on souhaite que l'eau ne soit pas au niveau 0 :)
{
    STRUC_CLIPVERTEX src[10];
    U32 i, j, xx, zz;
    S32 b;

    PtrMap = SkySeaTexture;
    RepMask = 0x7F7F;
    PtrCLUTGouraud = PtrCLUTFog;
    SetCLUT(PalLevel);

    b = 0;

    src[0].V_MapU = 0;               // xt0
    src[0].V_MapV = 0;               // yt0
    src[1].V_MapU = 0;               // xt1
    src[1].V_MapV = 127 * 256 + 255; // yt1
    src[2].V_MapU = 127 * 256 + 255; // xt2
    src[2].V_MapV = 127 * 256 + 255; // yt2
    src[3].V_MapU = 127 * 256 + 255; // xt3
    src[3].V_MapV = 0;               // yt3

    zz = 0;
    for (i = 0; i < NB_POLYS_COTE; i++, zz += WORLD_SIZE / NB_POLYS_COTE) {
        xx = 0;
        for (j = 0; j < NB_POLYS_COTE; j++, xx += WORLD_SIZE / NB_POLYS_COTE) {
            if (CubeBitField & (1 << b)) {
                if (!SkySeaQuadVisible(xx, zz, y,
                                       WORLD_SIZE / (NB_POLYS_COTE * 2))) {
                    b++;
                    continue;
                }
                LongWorldRotatePoint(xx, y, zz);
                src[0].V_X0 = X0 - CameraXr;
                src[0].V_Y0 = Y0 - CameraYr;
                src[0].V_Z0 = CameraZr - Z0;

                LongWorldRotatePoint(xx, y, zz + WORLD_SIZE / NB_POLYS_COTE);
                src[1].V_X0 = X0 - CameraXr;
                src[1].V_Y0 = Y0 - CameraYr;
                src[1].V_Z0 = CameraZr - Z0;

                LongWorldRotatePoint(xx + WORLD_SIZE / NB_POLYS_COTE, y, zz + WORLD_SIZE / NB_POLYS_COTE);
                src[2].V_X0 = X0 - CameraXr;
                src[2].V_Y0 = Y0 - CameraYr;
                src[2].V_Z0 = CameraZr - Z0;

                LongWorldRotatePoint(xx + WORLD_SIZE / NB_POLYS_COTE, y, zz);
                src[3].V_X0 = X0 - CameraXr;
                src[3].V_Y0 = Y0 - CameraYr;
                src[3].V_Z0 = CameraZr - Z0;

                Draw_Poly(src, 24);
            } else {
                // poly noir ?
                /* on verra		LongWorldRotatePoint( xx, y, zz ) ;
			src[0].V_X0    = X0-CameraXr ;
			src[0].V_Y0    = Y0-CameraYr ;
			src[0].V_Z0    = CameraZr-Z0 ;

			LongWorldRotatePoint( xx, y, zz+WORLD_SIZE/NB_POLYS_COTE ) ;
			src[1].V_X0    = X0-CameraXr ;
			src[1].V_Y0    = Y0-CameraYr ;
			src[1].V_Z0    = CameraZr-Z0 ;

			LongWorldRotatePoint( xx+WORLD_SIZE/NB_POLYS_COTE, y, zz+WORLD_SIZE/NB_POLYS_COTE ) ;
			src[2].V_X0    = X0-CameraXr ;
			src[2].V_Y0    = Y0-CameraYr ;
			src[2].V_Z0    = CameraZr-Z0 ;

			LongWorldRotatePoint( xx+WORLD_SIZE/NB_POLYS_COTE, y, zz ) ;
			src[3].V_X0    = X0-CameraXr ;
			src[3].V_Y0    = Y0-CameraYr ;
			src[3].V_Z0    = CameraZr-Z0 ;

			Draw_Poly(src,0) ;
*/
            }
            b++;
        }
    }
}
