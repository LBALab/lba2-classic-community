
/* Cheap sea/sky quad reject.  A quad that is wholly outside the frame
   cannot contribute a pixel; its four rotations/projections and filler call
   are pure sea-view CPU cost.  A failed projection or a near quad is kept
   and left to Draw_Poly's exact clipper. */
static S32 SkySeaQuadVisible(S32 x0, S32 z0, S32 y, S32 halfWorld) {
    LongWorldRotatePoint(x0 + halfWorld, y, z0 + halfWorld);
    if (!LongProjectPoint(X0, Y0, Z0)) {
        return 1;
    }
    const S32 depth = CameraZr - Z0;
    if (depth > ClipZFar) {
        return 0;
    }
    if (depth < NearClip) {
        return 1;
    }
    /* Use the nearest possible depth so the bound stays conservative for
       the near half of the quad; the exact clipper still handles it. */
    const S32 nearDepth = depth - halfWorld * 2;
    if (nearDepth <= 0) {
        return 1;
    }
    S32 halfX = (S32)((float)halfWorld * FRatioX / (float)nearDepth) + 3;
    S32 halfY = (S32)((float)halfWorld * FRatioY / (float)nearDepth) + 3;
    if (Xp + halfX < 0 || Xp - halfX > (S32)ModeDesiredX) {
        return 0;
    }
    if (Yp + halfY < 0 || Yp - halfY > (S32)ModeDesiredY) {
        return 0;
    }
    return 1;
}

S32 Draw_Poly(STRUC_CLIPVERTEX src[], S32 type) {
    S32 k, l;
    STRUC_CLIPVERTEX dst[10];
    STRUC_CLIPVERTEX dst1[10];

    k = ClipperZ(dst1, src, 4, NearClip + 1, 0);

    if (k) {
        k = ClipperZ(dst, dst1, k, Fill_Z_Fog_Far, -1);
        if (k) {
            for (l = 0; l != k; l++) {
                if (!LongProjectPoint(dst[l].V_X0 + CameraXr, dst[l].V_Y0 + CameraYr, CameraZr - dst[l].V_Z0))
                    return FALSE;
                static S32 s_noSeaClamp = -1;
                if (s_noSeaClamp < 0) {
                    s_noSeaClamp = getenv("LBA2_NO_SEA_CLAMP") ? 1 : 0;
                }
                if (GPURendererActive && !s_noSeaClamp) {
                    /* A large sea/sky tile can have a corner project far
                       outside S16; the GPU clips it, so clamping keeps the
                       tile instead of dropping it and leaving a hole. */
                    if (Xp < -32768) {
                        Xp = -32768;
                    } else if (Xp > 32767) {
                        Xp = 32767;
                    }
                    if (Yp < -32768) {
                        Yp = -32768;
                    } else if (Yp > 32767) {
                        Yp = 32767;
                    }
                } else if ((Xp != (S32)(S16)Xp) OR(Yp != (S32)(S16)Yp))
                    return FALSE;
                Tab_Points[l].Pt_XE = (S16)Xp;
                Tab_Points[l].Pt_YE = (S16)Yp;
                Tab_Points[l].Pt_Light = dst[l].V_Light;
                Tab_Points[l].Pt_MapU = dst[l].V_MapU;
                Tab_Points[l].Pt_MapV = dst[l].V_MapV;
                Tab_Points[l].Pt_W = INV64(dst[l].V_Z0);
                Tab_Points[l].Pt_ZO = GET_ZO(dst[l].V_Z0);
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
