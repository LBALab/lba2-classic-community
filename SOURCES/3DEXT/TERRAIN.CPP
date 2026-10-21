#include "DEFINES.H"
#include "../PERSO.H"

#include <SVGA/SCREEN.H>

#ifdef USE_GPURENDERER
#include <GL/RENDER_GL.H>
extern S32 GPURendererActive;
#endif

/* Shared object page: DoTextureAnimation() may rebake effects in it between
   water ticks.  Name it too so the page refresh keeps those effects current. */
extern U8 *BufferTexture;
/* DoTextureAnimation() runs while either flag is set. */
extern U8 AnimateTexture;
extern S32 ProtectActif;

#define LIMITE_LOIN (60 * 1024)
#define COLOR_LIMITE 255
#define SIZE_TEXTURE 64

#define WORLD_SIZE 32768
#define NB_POLYS_COTE 4

#include <SDL3/SDL.h>
#include <SYSTEM/LOG.H>

// Taille 2D mini pour accepter un poly animé
#define ANI_POLY_STEP_Y 2
#define ANI_POLY_STEP_X 5

#include "DRAWSKY.CPP"

S32 Sky_Y;
//S32	NbDrawnPolys ;

S32 FlagMasque = TRUE;

#if defined(DEBUG_TOOLS) && defined(LBA_GAME)
extern S32 DebugFps;
#endif

#ifndef LBA_GAME
S32 Reglage = 0;
#endif

#ifdef LBA_EDITOR // Special PERSO.EXE
#define INFO_DECORS 1024
extern S32 FlagInfos;
#endif

//****************************************************************************
//****************************************************************************

S32 ListTypePolygone[4] = {0, POLY_SOLID, POLY_GOURAUD_TABLE, POLY_DITHER_TABLE};
S32 ListTypePTexture[] = {
    0,
    POLY_TEXTURE,
    POLY_TEXTURE_FLAT,
    POLY_TEXTURE_GOURAUD,
    0,
    POLY_TEXTURE_INCRUST,
    POLY_TEXTURE_INCRUST_FLAT,
    POLY_TEXTURE_INCRUST_GOURAUD,
    0,
    POLY_TEXTURE_INCRUST,
    POLY_TEXTURE_INCRUST_FLAT,
    POLY_TEXTURE_INCRUST_GOURAUD,
    0,
    POLY_TEXTURE_INCRUST,
    POLY_TEXTURE_INCRUST_FLAT,
    POLY_TEXTURE_INCRUST_GOURAUD,
};

/*══════════════════════════════════════════════════════════════════════════*
		   ▀▀█▀▀ █▀▀▀▀ █▀▀▀█ █▀▀▀█ █▀▀▀█  █    ██▄ █
		     ██  ██▀▀  ██▀█▀ ██▀█▀ ██▀▀█  ██   ██▀██
		     ▀▀  ▀▀▀▀▀ ▀▀  ▀ ▀▀  ▀ ▀▀  ▀  ▀▀   ▀▀  ▀
 *══════════════════════════════════════════════════════════════════════════*/

/*──────────────────────────────────────────────────────────────────────────*/

void DrawSkySeaZBuf() {
    // draw Sky
    //	Sky_Y = 12500 ;

    PtrMap = SkySeaTexture;
    RepMask = 0x7F7F;

    DrawSky(Sky_Y);
    //	DrawSea( Y_SEA ) ;
}

/*──────────────────────────────────────────────────────────────────────────*/

/* clearLog: fill Log with the fog colour before drawing.  GL mode renders
   the terrain into the FBO, so the caller passes FALSE and Log is cleared
   once, wholesale, after the terrain (see AffGrilleExt). */
void ClsTerrainZBuf(S32 clearLog) {
    SetClearColor((U8)FogCoul);
    if (clearLog)
        Cls();
    SetClearColor(0);

    memset(PtrZBuffer, 255, SIZE_Z_BUFFER); // Clear ZBuffer
}

/*──────────────────────────────────────────────────────────────────────────*/
static inline void DrawFeuillePolyZBuf(S32 xi, S32 zi, T_SOMMET_ROT *ptrs) {
    S32 color;
    T_HALF_POLY *ptrh;
    S32 f0, f1, f2, f3;
    S32 i0, i1, i2, i3;
    S32 x0, z0;
    S32 x1, z1;
    S32 x2, z2;
    S32 x3, z3;
    U8 type;
    U16 fog;

    ptrh = &MapPolyGround[zi * (64 * 2) + xi * 2];

    // optimisation globale affichage terrain
    if (!(ptrh->PolyFlag + ptrh->TexFlag +
          (ptrh + 1)->PolyFlag + (ptrh + 1)->TexFlag)) {
        return;
    }

    x0 = ptrs[0].X2D;
    z0 = ptrs[0].Y2D;
    x1 = ptrs[65].X2D;
    z1 = ptrs[65].Y2D;
    x3 = ptrs[1].X2D;
    z3 = ptrs[1].Y2D;
    x2 = ptrs[66].X2D;
    z2 = ptrs[66].Y2D;

    f0 = ptrs[0].Zrot;

    /*	PtrCLUTGouraud = PtrCLUTFog +
		(16*256) * BoundRegleTrois(0,15,
					ClipZFar - StartZFog,
					f0 - StartZFog ) ;
*/
    fog = (U16)(PalLevel + 16 * BoundRegleTrois(0, 15,
                                                ClipZFar - StartZFog,
                                                f0 - StartZFog));
    SetCLUT(fog);

    f0 = GET_ZO(f0);
    f1 = GET_ZO(ptrs[65].Zrot);
    f3 = GET_ZO(ptrs[1].Zrot);
    f2 = GET_ZO(ptrs[66].Zrot);

    {
        U8 *i = &MapIntensity[zi * 65 + xi];

        i0 = (i[0] & 15) * 256 - 255;
        i1 = (i[65] & 15) * 256 - 255;
        i3 = (i[1] & 15) * 256 - 255;
        i2 = (i[65 + 1] & 15) * 256 - 255;

        if (i0 < 0)
            i0 = 0; // groumpf
        if (i1 < 0)
            i1 = 0; // groumpf
        if (i2 < 0)
            i2 = 0; // groumpf
        if (i3 < 0)
            i3 = 0; // groumpf

/*		i0 = i[0]*256 		;
		i1 = i[65]*256		;
		i3 = i[1]*256 		;
		i2 = i[65+1]*256 	;
*/	}

if (ptrh->Sens == 0) {
    if (ptrh->PolyFlag + ptrh->TexFlag) {
        // type 0 triangle 0----------------------------

        Tab_Points[0].Pt_XE = (S16)x0;
        Tab_Points[0].Pt_YE = (S16)z0;
        Tab_Points[1].Pt_XE = (S16)x1;
        Tab_Points[1].Pt_YE = (S16)z1;
        Tab_Points[2].Pt_XE = (S16)x2;
        Tab_Points[2].Pt_YE = (S16)z2;

        if (TestVuePoly(Tab_Points)) {
            color = (ptrh->Bank << 4) + 11;

            Tab_Points[0].Pt_Light = (S16)i0;
            Tab_Points[1].Pt_Light = (S16)i1;
            Tab_Points[2].Pt_Light = (S16)i2;

            Tab_Points[0].Pt_ZO = (U16)f0;
            Tab_Points[1].Pt_ZO = (U16)f1;
            Tab_Points[2].Pt_ZO = (U16)f2;

            if (ptrh->PolyFlag) {
                if (ptrh->PolyFlag == 1) {
                    color += ((i0 + i1 + i2) * 21845) >> 24; // *21845 >> 16 EQU / 3
                }

                Fill_Poly(ListTypePolygone[ptrh->PolyFlag],
                          color, 3,
                          Tab_Points);
            }

            if (ptrh->TexFlag) {
                T_HALF_TEX *ptrt;

                ptrt = &ListTexDef[ptrh->IndexTex];

                Tab_Points[0].Pt_MapU = ptrt->Tx0;
                Tab_Points[0].Pt_MapV = ptrt->Ty0;
                Tab_Points[1].Pt_MapU = ptrt->Tx1;
                Tab_Points[1].Pt_MapV = ptrt->Ty1;
                Tab_Points[2].Pt_MapU = ptrt->Tx2;
                Tab_Points[2].Pt_MapV = ptrt->Ty2;

                if (ptrh->TexFlag == 2) {
                    color = ((i0 + i1 + i2) * 21845) >> 24; // *21845 >> 16 EQU / 3
                }

                type = (U8)ListTypePTexture[ptrh->TexFlag + (ptrh->PolyFlag << 2)];

                if (Fill_Poly(type, color, 3, Tab_Points)) {
                    if (MaxPolySea AND(ptrh->CodeJeu & 12) == 12) { // hihi je suis un polygone animé...

                        if (NbAniPoly < MaxPolySea - 1) {
                            PtrAniPoly->X0 = (S16)Min3(x0, x1, x2);
                            PtrAniPoly->Y0 = (S16)Min3(z0, z1, z2);
                            PtrAniPoly->X1 = (S16)Max3(x0, x1, x2);
                            PtrAniPoly->Y1 = (S16)Max3(z0, z1, z2);

                            if (((PtrAniPoly->Y1 - PtrAniPoly->Y0) > ANI_POLY_STEP_Y)
                                    AND((PtrAniPoly->X1 - PtrAniPoly->X0) > ANI_POLY_STEP_X)) {
                                PtrAniPoly->Type = type;
                                PtrAniPoly->Color = (U8)color;

                                PtrAniPoly->Fog = fog;

                                PtrAniPoly->Tab_Points[0] = Tab_Points[0];
                                PtrAniPoly->Tab_Points[1] = Tab_Points[1];
                                PtrAniPoly->Tab_Points[2] = Tab_Points[2];

                                NbAniPoly++;
                                PtrAniPoly++;
                            }
                        }
#ifdef DEBUG_TOOLS
                        else {
                            Message("MaxPolySea Atteint !", TRUE);
                        }
#endif
                    }
                }
            }
        }
    }
    // type 0 triangle 1----------------------------------

    ptrh++;

    if (ptrh->PolyFlag + ptrh->TexFlag) {
        Tab_Points[0].Pt_XE = (S16)x2;
        Tab_Points[0].Pt_YE = (S16)z2;
        Tab_Points[1].Pt_XE = (S16)x3;
        Tab_Points[1].Pt_YE = (S16)z3;
        Tab_Points[2].Pt_XE = (S16)x0;
        Tab_Points[2].Pt_YE = (S16)z0;

        if (TestVuePoly(Tab_Points)) {
            color = (ptrh->Bank << 4) + 11;

            Tab_Points[0].Pt_Light = (S16)i2;
            Tab_Points[1].Pt_Light = (S16)i3;
            Tab_Points[2].Pt_Light = (S16)i0;

            Tab_Points[0].Pt_ZO = (U16)f2;
            Tab_Points[1].Pt_ZO = (U16)f3;
            Tab_Points[2].Pt_ZO = (U16)f0;

            if (ptrh->PolyFlag) {
                if (ptrh->PolyFlag == 1) {
                    color += ((i0 + i2 + i3) * 21845) >> 24; // *21845 >> 16 EQU / 3
                }
                Fill_Poly(ListTypePolygone[ptrh->PolyFlag],
                          color, 3,
                          Tab_Points);
            }

            if (ptrh->TexFlag) {
                T_HALF_TEX *ptrt;

                ptrt = &ListTexDef[ptrh->IndexTex];

                Tab_Points[0].Pt_MapU = ptrt->Tx0;
                Tab_Points[0].Pt_MapV = ptrt->Ty0;
                Tab_Points[1].Pt_MapU = ptrt->Tx1;
                Tab_Points[1].Pt_MapV = ptrt->Ty1;
                Tab_Points[2].Pt_MapU = ptrt->Tx2;
                Tab_Points[2].Pt_MapV = ptrt->Ty2;

                if (ptrh->TexFlag == 2) {
                    color = ((i0 + i2 + i3) * 21845) >> 24; // *21845 >> 16 EQU / 3
                }

                type = (U8)ListTypePTexture[ptrh->TexFlag + (ptrh->PolyFlag << 2)];

                if (Fill_Poly(type, color, 3, Tab_Points)) {
                    if (MaxPolySea AND(ptrh->CodeJeu & 12) == 12) { // hihi je suis un polygone animé...
                        if (NbAniPoly < MaxPolySea - 1) {
                            PtrAniPoly->X0 = (S16)Min3(x0, x2, x3);
                            PtrAniPoly->Y0 = (S16)Min3(z0, z2, z3);
                            PtrAniPoly->X1 = (S16)Max3(x0, x2, x3);
                            PtrAniPoly->Y1 = (S16)Max3(z0, z2, z3);

                            if (((PtrAniPoly->Y1 - PtrAniPoly->Y0) > ANI_POLY_STEP_Y)
                                    AND((PtrAniPoly->X1 - PtrAniPoly->X0) > ANI_POLY_STEP_X)) {
                                PtrAniPoly->Type = type;
                                PtrAniPoly->Color = (U8)color;

                                PtrAniPoly->Fog = fog;

                                PtrAniPoly->Tab_Points[0] = Tab_Points[0];
                                PtrAniPoly->Tab_Points[1] = Tab_Points[1];
                                PtrAniPoly->Tab_Points[2] = Tab_Points[2];

                                NbAniPoly++;
                                PtrAniPoly++;
                            }
                        }
#ifdef DEBUG_TOOLS
                        else {
                            Message("MaxPolySea Atteint !", TRUE);
                        }
#endif
                    }
                }
            }
        }
    }
} else // type == 1---------------------------------------------------
{
    // type 1 triangle 0

    if (ptrh->PolyFlag + ptrh->TexFlag) {
        Tab_Points[0].Pt_XE = (S16)x3;
        Tab_Points[0].Pt_YE = (S16)z3;
        Tab_Points[1].Pt_XE = (S16)x0;
        Tab_Points[1].Pt_YE = (S16)z0;
        Tab_Points[2].Pt_XE = (S16)x1;
        Tab_Points[2].Pt_YE = (S16)z1;

        if (TestVuePoly(Tab_Points)) {
            color = (ptrh->Bank << 4) + 11;

            Tab_Points[0].Pt_Light = (S16)i3;
            Tab_Points[1].Pt_Light = (S16)i0;
            Tab_Points[2].Pt_Light = (S16)i1;

            Tab_Points[0].Pt_ZO = (U16)f3;
            Tab_Points[1].Pt_ZO = (U16)f0;
            Tab_Points[2].Pt_ZO = (U16)f1;

            if (ptrh->PolyFlag) {
                if (ptrh->PolyFlag == 1) {
                    color += ((i0 + i1 + i3) * 21845) >> 24; // *21845 >> 16 EQU / 3
                }
                Fill_Poly(ListTypePolygone[ptrh->PolyFlag],
                          color, 3,
                          Tab_Points);
            }

            if (ptrh->TexFlag) {
                T_HALF_TEX *ptrt;

                ptrt = &ListTexDef[ptrh->IndexTex];

                Tab_Points[0].Pt_MapU = ptrt->Tx0;
                Tab_Points[0].Pt_MapV = ptrt->Ty0;
                Tab_Points[1].Pt_MapU = ptrt->Tx1;
                Tab_Points[1].Pt_MapV = ptrt->Ty1;
                Tab_Points[2].Pt_MapU = ptrt->Tx2;
                Tab_Points[2].Pt_MapV = ptrt->Ty2;

                if (ptrh->TexFlag == 2) {
                    color = ((i0 + i1 + i3) * 21845) >> 24; // *21845 >> 16 EQU / 3
                }

                type = (U8)ListTypePTexture[ptrh->TexFlag + (ptrh->PolyFlag << 2)];

                if (Fill_Poly(type, color, 3, Tab_Points)) {
                    if (MaxPolySea AND(ptrh->CodeJeu & 12) == 12) { // hihi je suis un polygone animé...
                        if (NbAniPoly < MaxPolySea - 1) {
                            PtrAniPoly->X0 = (S16)Min3(x0, x1, x3);
                            PtrAniPoly->Y0 = (S16)Min3(z0, z1, z3);
                            PtrAniPoly->X1 = (S16)Max3(x0, x1, x3);
                            PtrAniPoly->Y1 = (S16)Max3(z0, z1, z3);

                            if (((PtrAniPoly->Y1 - PtrAniPoly->Y0) > ANI_POLY_STEP_Y)
                                    AND((PtrAniPoly->X1 - PtrAniPoly->X0) > ANI_POLY_STEP_X)) {
                                PtrAniPoly->Type = type;
                                PtrAniPoly->Color = (U8)color;

                                PtrAniPoly->Fog = fog;

                                PtrAniPoly->Tab_Points[0] = Tab_Points[0];
                                PtrAniPoly->Tab_Points[1] = Tab_Points[1];
                                PtrAniPoly->Tab_Points[2] = Tab_Points[2];

                                NbAniPoly++;
                                PtrAniPoly++;
                            }
                        }
#ifdef DEBUG_TOOLS
                        else {
                            Message("MaxPolySea Atteint !", TRUE);
                        }
#endif
                    }
                }
            }
        }
    }
    // type 1 triangle 1

    ptrh++;

    if (ptrh->PolyFlag + ptrh->TexFlag) {
        Tab_Points[0].Pt_XE = (S16)x1;
        Tab_Points[0].Pt_YE = (S16)z1;
        Tab_Points[1].Pt_XE = (S16)x2;
        Tab_Points[1].Pt_YE = (S16)z2;
        Tab_Points[2].Pt_XE = (S16)x3;
        Tab_Points[2].Pt_YE = (S16)z3;

        if (TestVuePoly(Tab_Points)) {
            color = (ptrh->Bank << 4) + 11;

            Tab_Points[0].Pt_Light = (S16)i1;
            Tab_Points[1].Pt_Light = (S16)i2;
            Tab_Points[2].Pt_Light = (S16)i3;

            Tab_Points[0].Pt_ZO = (U16)f1;
            Tab_Points[1].Pt_ZO = (U16)f2;
            Tab_Points[2].Pt_ZO = (U16)f3;

            if (ptrh->PolyFlag) {
                if (ptrh->PolyFlag == 1) {
                    color += ((i1 + i2 + i3) * 21845) >> 24; // *21845 >> 16 EQU / 3
                }

                Fill_Poly(ListTypePolygone[ptrh->PolyFlag],
                          color, 3,
                          Tab_Points);
            }

            if (ptrh->TexFlag) {
                T_HALF_TEX *ptrt;

                ptrt = &ListTexDef[ptrh->IndexTex];

                Tab_Points[0].Pt_MapU = ptrt->Tx0;
                Tab_Points[0].Pt_MapV = ptrt->Ty0;
                Tab_Points[1].Pt_MapU = ptrt->Tx1;
                Tab_Points[1].Pt_MapV = ptrt->Ty1;
                Tab_Points[2].Pt_MapU = ptrt->Tx2;
                Tab_Points[2].Pt_MapV = ptrt->Ty2;

                if (ptrh->TexFlag == 2) {
                    color = ((i1 + i2 + i3) * 21845) >> 24; // *21845 >> 16 EQU / 3
                }

                type = (U8)ListTypePTexture[ptrh->TexFlag + (ptrh->PolyFlag << 2)];

                if (Fill_Poly(type, color, 3, Tab_Points)) {
                    if (MaxPolySea AND(ptrh->CodeJeu & 12) == 12) { // hihi je suis un polygone animé...
                        if (NbAniPoly < MaxPolySea - 1) {
                            PtrAniPoly->X0 = (S16)Min3(x1, x2, x3);
                            PtrAniPoly->Y0 = (S16)Min3(z1, z2, z3);
                            PtrAniPoly->X1 = (S16)Max3(x1, x2, x3);
                            PtrAniPoly->Y1 = (S16)Max3(z1, z2, z3);

                            if (((PtrAniPoly->Y1 - PtrAniPoly->Y0) > ANI_POLY_STEP_Y)
                                    AND((PtrAniPoly->X1 - PtrAniPoly->X0) > ANI_POLY_STEP_X)) {
                                PtrAniPoly->Type = type;
                                PtrAniPoly->Color = (U8)color;

                                PtrAniPoly->Fog = fog;

                                PtrAniPoly->Tab_Points[0] = Tab_Points[0];
                                PtrAniPoly->Tab_Points[1] = Tab_Points[1];
                                PtrAniPoly->Tab_Points[2] = Tab_Points[2];

                                NbAniPoly++;
                                PtrAniPoly++;
                            }
                        }
#ifdef DEBUG_TOOLS
                        else {
                            Message("MaxPolySea Atteint !", TRUE);
                        }
#endif
                    }
                }
            }
        }
    }
}

#if !(defined(DEMO) || defined(LBA_GAME))
if (FillFeuille & 4) {
    DrawColFeuille3D(x0, z0,
                     x1, z1,
                     x2, z2,
                     x3, z3,
                     MapPolyGround[zi * 64 * 2 + xi * 2].Sens,
                     CYAN,
                     MapPolyGround[zi * 64 * 2 + xi * 2].Col,
                     MapPolyGround[zi * 64 * 2 + xi * 2 + 1].Col);
}

if (FillFeuille & 2) {
    DrawFeuille3D(x0, z0,
                  x1, z1,
                  x2, z2,
                  x3, z3,
                  MapPolyGround[zi * 64 * 2 + xi * 2].Sens,
                  GREEN,
                  FALSE);
}
#endif
}

/*──────────────────────────────────────────────────────────────────────────*/
static inline void FillBlackPolyZBuf(S32 xi, S32 zi, T_SOMMET_ROT *ptrs, U8 *ptra) {
    T_HALF_POLY *ptrh;
    //	S32 		f0, f1, f2, f3 	;
    //	S32 		f00, f10, f20, f30 ;
    S32 x0, z0;
    S32 x1, z1;
    S32 x2, z2;
    S32 x3, z3;
    U8 b0, b1, b2, b3;

    ptrh = &MapPolyGround[zi * (64 * 2) + xi * 2];

    x0 = ptrs[0].X2D;
    z0 = ptrs[0].Y2D;
    b0 = (U8)(ptra[0] & 32);
    x1 = ptrs[65].X2D;
    z1 = ptrs[65].Y2D;
    b1 = (U8)(ptra[65] & 32);
    x3 = ptrs[1].X2D;
    z3 = ptrs[1].Y2D;
    b3 = (U8)(ptra[1] & 32);
    x2 = ptrs[66].X2D;
    z2 = ptrs[66].Y2D;
    b2 = (U8)(ptra[66] & 32);

    /*
	xi *= 512 ;
	zi *= 512 ;

	f0 = GET_ZO(ptrs[0].Zrot)	;
	f1 = GET_ZO(ptrs[65].Zrot)	;
	f3 = GET_ZO(ptrs[1].Zrot)	;
	f2 = GET_ZO(ptrs[66].Zrot)	;

	LongWorldRotatePoint(xi, 0, zi)	;
	f00 = GET_ZO(CameraZr - Z0)	;
	LongWorldRotatePoint(xi, 0, zi+512);
	f10 = GET_ZO(CameraZr - Z0)	;
	LongWorldRotatePoint(xi+512, 0, zi);
	f30 = GET_ZO(CameraZr - Z0)	;
	LongWorldRotatePoint(xi+512, 0, zi+512);
	f20 = GET_ZO(CameraZr - Z0)	;
*/

    if (!(b0 | b1)) {
        Tab_Points[0].Pt_XE = (S16)x0;
        Tab_Points[0].Pt_YE = (S16)z0;
        Tab_Points[1].Pt_XE = (S16)x1;
        Tab_Points[1].Pt_YE = (S16)z1;
        Tab_Points[2].Pt_XE = (S16)x1;
        Tab_Points[2].Pt_YE = (S16)((S32)ModeDesiredY - 1);
        Tab_Points[3].Pt_XE = (S16)x0;
        Tab_Points[3].Pt_YE = (S16)((S32)ModeDesiredY - 1);

        Tab_Points[0].Pt_Light = (S16)0;
        Tab_Points[1].Pt_Light = (S16)0;
        Tab_Points[2].Pt_Light = (S16)0;
        Tab_Points[3].Pt_Light = (S16)0;

        Tab_Points[0].Pt_ZO = (U16)0;
        Tab_Points[1].Pt_ZO = (U16)0;
        Tab_Points[2].Pt_ZO = (U16)0;
        Tab_Points[3].Pt_ZO = (U16)0;

        Fill_Poly(0, 0, 4, Tab_Points);
    }

    if (!(b1 | b2)) {
        Tab_Points[0].Pt_XE = (S16)x1;
        Tab_Points[0].Pt_YE = (S16)z1;
        Tab_Points[1].Pt_XE = (S16)x2;
        Tab_Points[1].Pt_YE = (S16)z2;
        Tab_Points[2].Pt_XE = (S16)x2;
        Tab_Points[2].Pt_YE = (S16)((S32)ModeDesiredY - 1);
        Tab_Points[3].Pt_XE = (S16)x1;
        Tab_Points[3].Pt_YE = (S16)((S32)ModeDesiredY - 1);

        Tab_Points[0].Pt_Light = (S16)0;
        Tab_Points[1].Pt_Light = (S16)0;
        Tab_Points[2].Pt_Light = (S16)0;
        Tab_Points[3].Pt_Light = (S16)0;

        Tab_Points[0].Pt_ZO = (U16)0;
        Tab_Points[1].Pt_ZO = (U16)0;
        Tab_Points[2].Pt_ZO = (U16)0;
        Tab_Points[3].Pt_ZO = (U16)0;

        Fill_Poly(0, 0, 4, Tab_Points);
    }

    if (!(b2 | b3)) {
        Tab_Points[0].Pt_XE = (S16)x2;
        Tab_Points[0].Pt_YE = (S16)z2;
        Tab_Points[1].Pt_XE = (S16)x3;
        Tab_Points[1].Pt_YE = (S16)z3;
        Tab_Points[2].Pt_XE = (S16)x3;
        Tab_Points[2].Pt_YE = (S16)((S32)ModeDesiredY - 1);
        Tab_Points[3].Pt_XE = (S16)x2;
        Tab_Points[3].Pt_YE = (S16)((S32)ModeDesiredY - 1);

        Tab_Points[0].Pt_Light = (S16)0;
        Tab_Points[1].Pt_Light = (S16)0;
        Tab_Points[2].Pt_Light = (S16)0;
        Tab_Points[3].Pt_Light = (S16)0;

        Tab_Points[0].Pt_ZO = (U16)0;
        Tab_Points[1].Pt_ZO = (U16)0;
        Tab_Points[2].Pt_ZO = (U16)0;
        Tab_Points[3].Pt_ZO = (U16)0;

        Fill_Poly(0, 0, 4, Tab_Points);
    }

    if (!(b3 | b0)) {
        Tab_Points[0].Pt_XE = (S16)x3;
        Tab_Points[0].Pt_YE = (S16)z3;
        Tab_Points[1].Pt_XE = (S16)x0;
        Tab_Points[1].Pt_YE = (S16)z0;
        Tab_Points[2].Pt_XE = (S16)x0;
        Tab_Points[2].Pt_YE = (S16)((S32)ModeDesiredY - 1);
        Tab_Points[3].Pt_XE = (S16)x3;
        Tab_Points[3].Pt_YE = (S16)((S32)ModeDesiredY - 1);

        Tab_Points[0].Pt_Light = (S16)0;
        Tab_Points[1].Pt_Light = (S16)0;
        Tab_Points[2].Pt_Light = (S16)0;
        Tab_Points[3].Pt_Light = (S16)0;

        Tab_Points[0].Pt_ZO = (U16)0;
        Tab_Points[1].Pt_ZO = (U16)0;
        Tab_Points[2].Pt_ZO = (U16)0;
        Tab_Points[3].Pt_ZO = (U16)0;

        Fill_Poly(0, 0, 4, Tab_Points);
    }
}
/*──────────────────────────────────────────────────────────────────────────*/
#ifdef USE_GPURENDERER
/* ── Near-plane tile clipping (GPU path) ────────────────────────────────── */

/* Rotated camera-space grid coordinates captured by the projection loop in
   AffichageTerrainZBuf (same 65×65 linear index as SommetRot).  The SW path
   only consumes the projected 2D result, but the near-plane clip below runs
   in this space, where the near plane is a simple constant-depth test. */
static S32 s_nearClipRX[65 * 65];
static S32 s_nearClipRY[65 * 65];
static S32 s_nearClipRZ[65 * 65];


/* One clipped vertex: camera-space position plus the per-vertex attributes
   the ground fillers consume (light, atlas UV). */
typedef struct {
    S32 rx, ry, rz;
    S32 light;
    S32 u, v;
} T_NEARCLIP_VERT;

/* Side-plane clip: projected X/Y of a surviving vertex must stay within a
   margin of the screen.  A tile clipped by the near plane alone can keep
   corners tens of thousands of pixels off to one side (camera-space lateral
   distance of the whole terrain grid, depth near the clip plane); at grazing
   or near-level camera pitches those corners project into full-screen
   textured wedges that smear the frame. */
#define NC_SCREEN_MARGIN 256
#define NC_MAX_OUT 10
static S32 s_ncKx16; /* |Xp - XCentre| <= zr * Kx (16.16 of LFactorX) */
static S32 s_ncKy16;

static S32 NC_PlaneDist(const T_NEARCLIP_VERT *p, S32 plane) {
    S32 dx = p->rx - CameraXr;
    S32 dy = p->ry - CameraYr;
    S32 zr = CameraZr - p->rz;
    S32 bx = (S32)(((S64)s_ncKx16 * zr) >> 16);
    S32 by = (S32)(((S64)s_ncKy16 * zr) >> 16);
    switch (plane) {
    case 0:
        return dx + bx; /* left:  dx >= -Kx*zr */
    case 1:
        return bx - dx; /* right: dx <= +Kx*zr */
    case 2:
        return dy + by; /* bottom */
    default:
        return by - dy; /* top */
    }
}

/* Sutherland-Hodgman pass of out[] against frustum side plane `plane`. */
static S32 NC_ClipPass(T_NEARCLIP_VERT *out, S32 nOut, S32 plane) {
    T_NEARCLIP_VERT tmp[NC_MAX_OUT];
    S32 n = 0;
    S32 e;

    for (e = 0; e < nOut; e++) {
        const T_NEARCLIP_VERT *P = &out[e];
        const T_NEARCLIP_VERT *Q = &out[(e + 1) % nOut];
        S32 fP = NC_PlaneDist(P, plane);
        S32 fQ = NC_PlaneDist(Q, plane);
        S32 inP = (fP >= 0);
        S32 inQ = (fQ >= 0);

        if (inP && n < NC_MAX_OUT) {
            tmp[n++] = *P;
        }
        if (inP != inQ && n < NC_MAX_OUT) {
            T_NEARCLIP_VERT *m = &tmp[n++];
            S32 t16 = (S32)(((S64)fP << 16) / (fP - fQ));
            m->rx = P->rx + (S32)(((S64)(Q->rx - P->rx) * t16) >> 16);
            m->ry = P->ry + (S32)(((S64)(Q->ry - P->ry) * t16) >> 16);
            m->rz = P->rz + (S32)(((S64)(Q->rz - P->rz) * t16) >> 16);
            m->light = P->light + (((Q->light - P->light) * t16) >> 16);
            m->u = P->u + (((Q->u - P->u) * t16) >> 16);
            m->v = P->v + (((Q->v - P->v) * t16) >> 16);
        }
    }
    memcpy(out, tmp, (size_t)n * sizeof(T_NEARCLIP_VERT));
    return n;
}

/* Clip one ground half-triangle against the camera near plane (keep
   CameraZr - rz >= NearClip) and draw the surviving part, replicating the
   DrawFeuillePolyZBuf attribute pipeline: same fog/CLUT selection, same
   ListTypePolygone / ListTypePTexture dispatch, backface cull included.
   The SW keeps its black-curtain fill for these tiles because its fillers
   cannot rasterize a polygon whose vertices sit behind the near plane; the
   GPU path can, and the ground the camera flies over stays visible instead
   of punching black holes into the view. */
static void FillNearClippedHalf(T_NEARCLIP_VERT *tri, U32 bank, U32 texFlag,
                                U32 polyFlag) {
    T_NEARCLIP_VERT out[NC_MAX_OUT];
    S32 nOut = 0;
    S32 e;

    for (e = 0; e < 3; e++) {
        const T_NEARCLIP_VERT *P = &tri[e];
        const T_NEARCLIP_VERT *Q = &tri[(e + 1) % 3];
        S32 zrP = CameraZr - P->rz;
        S32 zrQ = CameraZr - Q->rz;
        S32 inP = (zrP >= NearClip);
        S32 inQ = (zrQ >= NearClip);

        if (inP && nOut < NC_MAX_OUT) {
            out[nOut++] = *P;
        }
        if (inP != inQ && nOut < NC_MAX_OUT) {
            T_NEARCLIP_VERT *m = &out[nOut++];
            S32 t16 = (S32)(((S64)(zrP - NearClip) << 16) / (zrP - zrQ));
            m->rx = P->rx + (S32)(((S64)(Q->rx - P->rx) * t16) >> 16);
            m->ry = P->ry + (S32)(((S64)(Q->ry - P->ry) * t16) >> 16);
            m->rz = P->rz + (S32)(((S64)(Q->rz - P->rz) * t16) >> 16);
            /* Fixed-point rounding can land the intersection a unit behind
               the near plane, where LongProjectPoint refuses the vertex and
               the whole clipped triangle would drop out as a hole.  Snap it
               onto the plane. */
            if (CameraZr - m->rz < NearClip)
                m->rz = CameraZr - NearClip;
            m->light = P->light + (((Q->light - P->light) * t16) >> 16);
            m->u = P->u + (((Q->u - P->u) * t16) >> 16);
            m->v = P->v + (((Q->v - P->v) * t16) >> 16);
        }
    }

    for (e = 0; e < 4 && nOut > 0; e++) {
        nOut = NC_ClipPass(out, nOut, e);
    }

    for (e = 1; e + 1 < nOut; e++) {
        T_NEARCLIP_VERT *t[3];
        Struc_Point pts[3];
        S32 lightSum = 0;
        S32 ok = 1;
        S32 k;

        t[0] = &out[0];
        t[1] = &out[e];
        t[2] = &out[e + 1];

        for (k = 0; k < 3; k++) {
            S32 zr = CameraZr - t[k]->rz;
            if (!LongProjectPoint(t[k]->rx, t[k]->ry, t[k]->rz)) {
                ok = 0;
                break;
            }
            pts[k].Pt_XE = (S16)Xp;
            pts[k].Pt_YE = (S16)Yp;
            pts[k].Pt_Light = (S16)t[k]->light;
            pts[k].Pt_ZO = GET_ZO(zr);
            lightSum += t[k]->light;
        }
        if (!ok || !TestVuePoly(pts)) {
            continue;
        }

        S32 fog = (S32)(PalLevel + 16 * BoundRegleTrois(0, 15,
                                                       ClipZFar - StartZFog,
                                                       (CameraZr - ((t[0]->rz + t[1]->rz + t[2]->rz) / 3)) - StartZFog));
        SetCLUT((U16)fog);

        S32 color = (S32)((bank << 4) + 11);

        if (polyFlag) {
            S32 pcolor = color;
            if (polyFlag == 1) {
                pcolor += (lightSum * 21845) >> 24;
            }
            Fill_Poly(ListTypePolygone[polyFlag], pcolor, 3, pts);
        }

        if (texFlag) {
            for (k = 0; k < 3; k++) {
                pts[k].Pt_MapU = (U16)t[k]->u;
                pts[k].Pt_MapV = (U16)t[k]->v;
            }
            S32 tcolor = color;
            if (texFlag == 2) {
                tcolor += (lightSum * 21845) >> 24;
            }
            Fill_Poly((U8)ListTypePTexture[texFlag + (polyFlag << 2)],
                      tcolor, 3, pts);
        }
    }
}

/* GL mode replacement for FillBlackPolyZBuf on tiles that cross the near
   plane: both halves are clipped against the near plane and their surviving
   parts drawn with full texture/light/depth attributes.  The black curtains
   stay suppressed for these tiles — the void closer to the camera than the
   clipped edge is the FBO clear colour, exactly what the curtains painted. */
static void DrawNearClippedTileZBuf(S32 xi, S32 zi, T_SOMMET_ROT *ptrs, U8 *ptra) {
    T_HALF_POLY *ptrh = &MapPolyGround[zi * (64 * 2) + xi * 2];
    /* Corner order matches DrawFeuillePolyZBuf: [0] TL, [65] BL, [66] BR,
       [1] TR — half 0 = (TL, BL, BR), half 1 = (BR, TR, TL). */
    static const S32 cornOff[4] = {0, 65, 66, 1};
    T_NEARCLIP_VERT corn[4];
    S32 base = zi * 65 + xi;
    S32 k;

    /* Projection: |Xp - XCentre| = |dx| * LFactorX / zr, so bounding the
       projected offset to the screen + margin bounds |dx| to zr * K. */
    s_ncKx16 = (S32)(((S64)((ModeDesiredX / 2 + NC_SCREEN_MARGIN) << 16)
                      / LFactorX));
    s_ncKy16 = (S32)(((S64)((ModeDesiredY / 2 + NC_SCREEN_MARGIN) << 16)
                      / LFactorY));

    (void)ptrs;
    (void)ptra;

    for (k = 0; k < 4; k++) {
        S32 idx = base + cornOff[k];
        S32 v = (MapIntensity[idx] & 15) * 256 - 255;
        if (v < 0) {
            v = 0;
        }
        corn[k].rx = s_nearClipRX[idx];
        corn[k].ry = s_nearClipRY[idx];
        corn[k].rz = s_nearClipRZ[idx];
        corn[k].light = v;
        corn[k].u = 0;
        corn[k].v = 0;
    }

    if (ptrh[0].PolyFlag || ptrh[0].TexFlag) {
        T_NEARCLIP_VERT tri[3];
        T_HALF_TEX *ptrt = &ListTexDef[ptrh[0].IndexTex];
        tri[0] = corn[0];
        tri[1] = corn[1];
        tri[2] = corn[2];
        tri[0].u = ptrt->Tx0;
        tri[0].v = ptrt->Ty0;
        tri[1].u = ptrt->Tx1;
        tri[1].v = ptrt->Ty1;
        tri[2].u = ptrt->Tx2;
        tri[2].v = ptrt->Ty2;
        FillNearClippedHalf(tri, ptrh[0].Bank, ptrh[0].TexFlag,
                            ptrh[0].PolyFlag);
    }

    ptrh++;

    if (ptrh[0].PolyFlag || ptrh[0].TexFlag) {
        T_NEARCLIP_VERT tri[3];
        T_HALF_TEX *ptrt = &ListTexDef[ptrh[0].IndexTex];
        tri[0] = corn[2];
        tri[1] = corn[3];
        tri[2] = corn[0];
        tri[0].u = ptrt->Tx0;
        tri[0].v = ptrt->Ty0;
        tri[1].u = ptrt->Tx1;
        tri[1].v = ptrt->Ty1;
        tri[2].u = ptrt->Tx2;
        tri[2].v = ptrt->Ty2;
        FillNearClippedHalf(tri, ptrh[0].Bank, ptrh[0].TexFlag,
                            ptrh[0].PolyFlag);
    }
}
#endif /* USE_GPURENDERER */

/*──────────────────────────────────────────────────────────────────────────*/
struct S_Terrain {
    S8 x;
    S8 y;
};

struct S_Terrain TerrNO[9] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

struct S_Terrain TerrNE[9] = {
    {1, -1}, {0, -1}, {-1, -1}, {1, 0}, {0, 0}, {-1, 0}, {1, 1}, {0, 1}, {-1, 1}};

struct S_Terrain TerrSO[9] = {
    {-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {0, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};

struct S_Terrain TerrSE[9] = {
    {1, 1}, {0, 1}, {-1, 1}, {1, 0}, {0, 0}, {-1, 0}, {1, -1}, {0, -1}, {-1, -1}};

struct S_Terrain *TerrainTri;

S32 OffPosCamera[3] = {
    // offsets camera precalculés
    -(512 * 64), // -1
    0,           //  0
    (512 * 64)   //  1
};

extern S32 PtrProjectPoint(S32 xw, S32 yw, S32 zw);

/* Set when a horizon cube had to fall back to the sea placeholder this
 * frame (cube not streamed in yet): the backdrop is still changing. */
static S32 s_hznSeaDrew = 0;

#ifdef USE_GPURENDERER
/* Eye 0 rotates each horizon cube's 65x65 vertex grid into camera space for
 * projection.  Eye 1 only differs by the camera-space eye shift, which
 * LongProjectPoint applies through CameraXr/CameraYr, so the rotated points
 * are reused and only the per-eye projection and clipping run again.  Slots
 * are keyed by the 3x3 neighbor offset; the pair token rejects stale data. */
#define HZN_CACHE_SLOTS 9
#define HZN_CACHE_VERTS ((NB_COTE + 1) * (NB_COTE + 1))
static S32 s_hznRotX[HZN_CACHE_SLOTS][HZN_CACHE_VERTS];
static S32 s_hznRotY[HZN_CACHE_SLOTS][HZN_CACHE_VERTS];
static S32 s_hznRotZ[HZN_CACHE_SLOTS][HZN_CACHE_VERTS];
static U8 s_hznRotValid[HZN_CACHE_SLOTS];
static S32 s_hznRotSlot = -1;
static S32 s_hznRotEye = -1;
static U32 s_hznRotSeq = 0;
static S32 s_hznRotAlpha = 0, s_hznRotBeta = 0, s_hznRotGamma = 0;
static S32 s_hznRotBetaCam = 0;
static S32 s_hznRotCubeX = 0, s_hznRotCubeY = 0;
static S32 s_hznRotTypeProj = 0;
static S32 s_hznCacheActive = 0;
#endif

/* Conservative whole-cube frustum reject for the horizon backdrop.  The
 * camera sits inside the neighbour-cube grid, so cubes regularly fall
 * entirely outside the view cone and their 65x65 vertex transform is pure
 * waste.  A cube is skipped only when every corner of its padded bounding
 * box fails the SAME clip plane — for a convex box that is an exact test,
 * so a cube with any visible pixel is never culled.  Runs after
 * SetPosCamera has shifted the camera onto this cube and uses the same
 * rotate/project/clip sequence as the per-vertex loop below; the projection
 * globals are clobbered, which is fine because AffichageTerrainZBuf (or the
 * skip) re-derives everything. */
static S32 HorizonCubeOffscreen(void) {
    const S32 CUBE = -OffPosCamera[0];
    const S32 Y_MIN = -32768; /* full S16 height range: never cull a real peak */
    const S32 Y_MAX = 32767;
    static const S32 corner[8][3] = {
        {0, Y_MIN, 0}, {CUBE, Y_MIN, 0}, {0, Y_MIN, CUBE}, {CUBE, Y_MIN, CUBE},
        {0, Y_MAX, 0}, {CUBE, Y_MAX, 0}, {0, Y_MAX, CUBE}, {CUBE, Y_MAX, CUBE}};
    S32 common = 63; /* every clip plane is a cull candidate */
    S32 i;
    for (i = 0; i < 8; i++) {
        S32 flags;
        S32 zr;
        LongWorldRotatePoint(corner[i][0], corner[i][1], corner[i][2]);
        zr = CameraZr - Z0;
        if (zr <= NearClip) {
            flags = 1; /* behind the near plane */
        } else if (zr >= ClipZFar) {
            flags = 2; /* beyond the far plane */
        } else if (!LongProjectPoint(X0, Y0, Z0)) {
            flags = 1; /* unprojectable: treat like near-clipped */
        } else if (Xp < ClipXMin) {
            flags = 4; /* fully left of the screen */
        } else if (Xp > ClipXMax) {
            flags = 8;
        } else if (Yp < ClipYMin) {
            flags = 16;
        } else if (Yp > ClipYMax) {
            flags = 32;
        } else {
            flags = 0; /* this corner is on screen */
        }
        common &= flags;
        if (!common) {
            return 0; /* corners disagree: the cube may be visible */
        }
    }
    return 1;
}

void DrawHorizon2ZBuf() {
    S32 flag = FALSE;
    S32 cx = CameraX;
    S32 cy = CameraY;
    S32 cz = CameraZ;
    s_hznSeaDrew = 0;

#ifdef USE_GPURENDERER
    S32 hzEye = GL_GetStereoEye();
    s_hznRotSlot = -1;
    s_hznCacheActive = 0;
    if (GPURendererActive && hzEye == 0) {
        memset(s_hznRotValid, 0, sizeof(s_hznRotValid));
        s_hznRotAlpha = CameraAlpha;
        s_hznRotBeta = CameraBeta;
        s_hznRotGamma = CameraGamma;
        s_hznRotBetaCam = BetaCam;
        s_hznRotCubeX = CurrentCubeX;
        s_hznRotCubeY = CurrentCubeY;
        s_hznRotTypeProj = TypeProj;
        s_hznRotSeq = GL_GetStereoFrameSeq();
        s_hznRotEye = 0;
        s_hznCacheActive = 1;
    } else if (GPURendererActive && hzEye == 1 && s_hznRotEye == 0 &&
               s_hznRotSeq == GL_GetStereoFrameSeq() &&
               s_hznRotAlpha == CameraAlpha && s_hznRotBeta == CameraBeta &&
               s_hznRotGamma == CameraGamma && s_hznRotBetaCam == BetaCam &&
               s_hznRotCubeX == CurrentCubeX &&
               s_hznRotCubeY == CurrentCubeY &&
               s_hznRotTypeProj == TypeProj) {
        s_hznCacheActive = 1;
    } else {
        s_hznRotEye = -1;
    }
#endif

    if (BetaCam >= 0 AND BetaCam < 1024) {
        TerrainTri = TerrSO;
    } else if (BetaCam >= 1024 AND BetaCam < 2048) {
        TerrainTri = TerrNO;
    } else if (BetaCam >= 2048 AND BetaCam < 3072) {
        TerrainTri = TerrNE;
    } else if (BetaCam >= 3072 AND BetaCam < 4096) {
        TerrainTri = TerrSE;
    }
    /*
	if( BetaCam>=0 AND BetaCam<1024 )
	{
		TerrainTri = TerrNE ;
	}
	else if( BetaCam>=1024 AND BetaCam<2048 )
	{
		TerrainTri = TerrSE ;
	}
	else if( BetaCam>=2048 AND BetaCam<3072 )
	{
		TerrainTri = TerrSO ;
	}
	else if( BetaCam>=3072 AND BetaCam<4096 )
	{
		TerrainTri = TerrNO ;
	}
*/
    for (S32 n = 9; n > 0; n--, TerrainTri++) {
        S32 x, y;

        x = CurrentCubeX + TerrainTri->x;
        y = CurrentCubeY + TerrainTri->y;

        if (x >= 0 AND x <= 15 AND y >= 0 AND y <= 15) {
            if (TerrainTri->x OR TerrainTri->y) {
                // PreClipping du Cube
                /*				if( PtrProjectPoint( 0, 0, 0 )
				OR  PtrProjectPoint( 32767, 0, 0 )
				OR  PtrProjectPoint( 32767, 0, 32767 )
				OR  PtrProjectPoint( 0, 0, 32767 ) )
*/
                {
#ifdef LBA_GAME
                    SetPosCamera(cx - OffPosCamera[TerrainTri->x + 1] + TerrainTri->x, cy, cz - OffPosCamera[TerrainTri->y + 1] + TerrainTri->y);
#else
                    SetPosCamera(cx - OffPosCamera[TerrainTri->x + 1] + TerrainTri->x * Reglage, cy, cz - OffPosCamera[TerrainTri->y + 1] + TerrainTri->y * Reglage);
#endif
                    if (LoadCube(x, y)) {
                        if (!HorizonCubeOffscreen()) {
#ifdef USE_GPURENDERER
                            s_hznRotSlot =
                                (TerrainTri->x + 1) * 3 + (TerrainTri->y + 1);
#endif
                            AffichageTerrainZBuf();
                        }
                        flag = TRUE;
                    } else {
                        DrawOneSea(Y_SEA);
                        s_hznSeaDrew = 1;
                    }
                }
            }
        }
    }

    SetPosCamera(cx, cy, cz);

    if (flag) {
        if (!LoadCube(CurrentCubeX, CurrentCubeY)) {
            // on est dans un cube phantom !!!!!!!
            InitSeaCube();
            DrawOneSea(Y_SEA);
            s_hznSeaDrew = 1;
        }
    }

#ifdef USE_GPURENDERER
    s_hznCacheActive = 0;
    s_hznRotSlot = -1;
    if (hzEye != 0) {
        s_hznRotEye = -1;
    }
#endif
}

/* 1 when the last DrawHorizon2ZBuf rendered terrain for every candidate
 * cube — no sea placeholder drew, so the backdrop is in its steady state.
 * The stereo capture (GL_CaptureHorizon) may only reuse a previous capture
 * while this holds. */
S32 HorizonBackdropStable(void) {
    return !s_hznSeaDrew;
}

/*──────────────────────────────────────────────────────────────────────────*/
U32 MaskVisible(U8 *in_ptr) {
    // This code conversion could be improved...

    U8 *firstRow = in_ptr;
    U8 *secondRow = in_ptr + 65;

    U8 maskH = *firstRow;
    U8 maskL = *firstRow;

    maskH |= *secondRow;
    maskL &= *secondRow;

    maskH |= *(firstRow + 1);
    maskL &= *(firstRow + 1);

    maskH |= *(secondRow + 1);
    maskL &= *(secondRow + 1);

    U32 finalMask = 0;
    finalMask |= (maskH << 8);
    finalMask |= maskL;
    finalMask &= 0x20FF;

    return finalMask;
}

/*──────────────────────────────────────────────────────────────────────────*/
void AffichageTerrainZBuf(void) {
    S16 *ptrm;
    U8 *ptra;
    T_SOMMET_ROT *ptrs;
    U32 tRot0, tRot1;

    tRot0 = SDL_GetTicks();

    // affichage des objets du decors
    AffichageObjetDecorsZBuf();

    tRot1 = SDL_GetTicks();

    // calcul les coordonnées 2D de chaque points
    ptra = &SommetAffichable[0][0];
    ptrm = MapSommetY;
    ptrs = &SommetRot[0][0];

    S32 rotIdx = 0;
    S32 hzEyeLog = -1;
#ifdef USE_GPURENDERER
    hzEyeLog = GL_GetStereoEye();
    S32 hzCacheSlot = s_hznRotSlot;
    S32 hzUseCache = s_hznCacheActive && GL_GetStereoEye() == 1 &&
                     hzCacheSlot >= 0 && hzCacheSlot < HZN_CACHE_SLOTS &&
                     s_hznRotValid[hzCacheSlot];
    S32 hzStoreCache = s_hznCacheActive && GL_GetStereoEye() == 0 &&
                       hzCacheSlot >= 0 && hzCacheSlot < HZN_CACHE_SLOTS;
#endif
    for (S32 z = 0; z <= NB_COTE * 512; z += 512) {
        for (S32 x = 0; x <= NB_COTE * 512; x += 512) {
            S32 zr;

#ifdef USE_GPURENDERER
            if (hzUseCache) {
                X0 = s_hznRotX[hzCacheSlot][rotIdx];
                Y0 = s_hznRotY[hzCacheSlot][rotIdx];
                Z0 = s_hznRotZ[hzCacheSlot][rotIdx];
            } else {
                LongWorldRotatePoint(x, *ptrm, z);
                if (hzStoreCache) {
                    s_hznRotX[hzCacheSlot][rotIdx] = X0;
                    s_hznRotY[hzCacheSlot][rotIdx] = Y0;
                    s_hznRotZ[hzCacheSlot][rotIdx] = Z0;
                }
            }
            /* Camera-space position kept for the near-plane tile clip. */
            s_nearClipRX[rotIdx] = X0;
            s_nearClipRY[rotIdx] = Y0;
            s_nearClipRZ[rotIdx] = Z0;
            rotIdx++;
#else
            LongWorldRotatePoint(x, *ptrm, z);
#endif

            zr = CameraZr - Z0;

            // cone clipping

            if (zr <= NearClip) {
                *ptra = 32;
            } else if ((zr < ClipZFar)
                           AND LongProjectPoint(X0, Y0, Z0)) {
                U8 flag;

                flag = 16;

                if (Xp < ClipXMin)
                    flag |= 1;
                if (Xp > ClipXMax)
                    flag |= 2;
                if (Yp < ClipYMin)
                    flag |= 4;
                if (Yp > ClipYMax)
                    flag |= 8;

                *ptra = flag;

                ptrs->Zrot = zr;

                ptrs->X2D = (S16)Xp;
                ptrs->Y2D = (S16)Yp;

            } else {
                *ptra = 0;
            }

            ptra++;
            ptrm++;
            ptrs++;
        }
    }

#ifdef USE_GPURENDERER
    if (hzStoreCache) {
        s_hznRotValid[hzCacheSlot] = 1;
    }
#endif

    PtrMap = GroundTexture;
    RepMask = 0xFFFF;

    {
        U32 tRot2 = SDL_GetTicks();
        static S32 s_rotLog = 0;
        if (s_rotLog < 40) {
            Log_Info("TerrainSplit2: eye=%d obj=%dms rot=%dms", hzEyeLog,
                     (S32)(tRot1 - tRot0), (S32)(tRot2 - tRot1));
            s_rotLog++;
        }
    }

    /* Draw the visible tiles sorted by a state key instead of in scan
       order.  DrawFeuillePolyZBuf calls SetCLUT(fog) per tile, and the GL
       batch flushes whenever the type, flat colour or CLUT row changes —
       scan order mixes fog distances, banks and textured/flat tiles
       constantly, emitting thousands of tiny draw calls per cube
       (hundreds of ms on mobile GPUs).  The key packs the full batch state
       (fog level, both halves' Tex/PolyFlag classes and banks), so tiles
       drawn consecutively share one flush state.  All terrain fillers are
       z-buffered and unblended (solid, stipple or chroma-keyed), so the
       draw order does not change the result. */
    {
        /* key: fog(16) | class(h0.Tex,h1.Tex,h0.Poly,h1.Poly, 2b each) |
           bank(h0,h1, 4b each) — one bit-pattern per GL batch state. */
        typedef struct {
            U32 key;
            S16 idx;
        } T_TILE_SORT;
        static T_TILE_SORT s_tiles[NB_COTE * NB_COTE];
        S32 tileCount = 0;

        U32 tb0 = SDL_GetTicks();

        ptra = &SommetAffichable[0][0];
        ptrs = &SommetRot[0][0];

        for (S32 z = 0; z < NB_COTE; z++) {
            for (S32 x = 0; x < NB_COTE; x++) {
                S32 mask;

                mask = MaskVisible(ptra);

                switch (mask & 0xFF) {
                case 32:
                    break;
                case 16: {
                    /* Same fog level computation as DrawFeuillePolyZBuf */
                    S32 fogLevel = BoundRegleTrois(0, 15,
                                                   ClipZFar - StartZFog,
                                                   ptrs[0].Zrot - StartZFog);
                    T_HALF_POLY *h = &MapPolyGround[z * (64 * 2) + x * 2];
                    s_tiles[tileCount].key =
                        ((U32)fogLevel << 24) |
                        ((U32)(h->TexFlag & 3) << 20) |
                        ((U32)((h + 1)->TexFlag & 3) << 18) |
                        ((U32)(h->PolyFlag & 3) << 16) |
                        ((U32)((h + 1)->PolyFlag & 3) << 14) |
                        ((U32)(h->Bank & 15) << 10) |
                        ((U32)((h + 1)->Bank & 15) << 6);
                    s_tiles[tileCount].idx = (S16)(z * (NB_COTE + 1) + x);
                    tileCount++;
                    break;
                }
                case 0:
                    if (mask) {
#ifdef USE_GPURENDERER
                        if (GPURendererActive && (mask & 0x2000)) {
                            /* Some corner crosses the near plane: draw the
                             * clipped remainder instead of black curtains. */
                            DrawNearClippedTileZBuf(x, z, ptrs, ptra);
                        } else {
                            FillBlackPolyZBuf(x, z, ptrs, ptra);
                        }
#else
                        FillBlackPolyZBuf(x, z, ptrs, ptra);
#endif
                    }
                    break;
                }

                ptra++;
                ptrs++;
            }

            ptra++;
            ptrs++;
        }

        /* Ascending sort by state key: LSD counting sort on the four key
           bytes.  The keys repeat heavily at grid-wide tile counts, so
           qsort's indirect comparator calls cost real time every frame;
           the keys fit a byte-wise radix perfectly.  Stable, so equal keys
           keep scan order (draw order among equal-state tiles is
           irrelevant: terrain fillers are opaque and z-buffered). */
        {
            static T_TILE_SORT s_sortScratch[NB_COTE * NB_COTE];
            static S32 s_counts[256];
            T_TILE_SORT *src = s_tiles;
            T_TILE_SORT *dst = s_sortScratch;
            for (S32 pass = 0; pass < 4; pass++) {
                memset(s_counts, 0, sizeof(s_counts));
                for (S32 i = 0; i < tileCount; i++) {
                    s_counts[(src[i].key >> (pass * 8)) & 0xFF]++;
                }
                S32 sum = 0;
                for (S32 b = 0; b < 256; b++) {
                    S32 c = s_counts[b];
                    s_counts[b] = sum;
                    sum += c;
                }
                for (S32 i = 0; i < tileCount; i++) {
                    dst[s_counts[(src[i].key >> (pass * 8)) & 0xFF]++] =
                        src[i];
                }
                T_TILE_SORT *t = src;
                src = dst;
                dst = t;
            }
            if (src != s_tiles) {
                memcpy(s_tiles, src,
                       (size_t)tileCount * sizeof(T_TILE_SORT));
            }
        }

        U32 tb1 = SDL_GetTicks();

        for (S32 i = 0; i < tileCount; i++) {
            S32 idx = s_tiles[i].idx;
            DrawFeuillePolyZBuf(idx % (NB_COTE + 1), idx / (NB_COTE + 1),
                                &SommetRot[0][0] + idx);
        }

        {
            static S32 s_splitLog = 0;
            if (s_splitLog < 40) {
                Log_Info("TerrainSplit: bucket=%dms draw=%dms",
                         (S32)(tb1 - tb0), (S32)(SDL_GetTicks() - tb1));
                s_splitLog++;
            }
        }
    }

    {
        U32 tSea0 = SDL_GetTicks();
        DrawOneSea(Y_SEA);
        U32 tSea1 = SDL_GetTicks();
        static S32 s_seaLog = 0;
        if (s_seaLog < 40) {
            Log_Info("TerrainSplit3: presea=%dms sea=%dms",
                     (S32)(tSea0 - tRot1), (S32)(tSea1 - tSea0));
            s_seaLog++;
        }
    }

#ifdef LBA_EDITOR
    if (FlagInfos & INFO_DECORS) {
        AffichageObjetDecorsZV();
    }
#endif

    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);
}

/*

void	AffichageTerrainZBuf( void )
{
	S16		*ptrm	;
	U8		*ptra	;
	T_SOMMET_ROT	*ptrs	;

	// affichage des objets du decors
	AffichageObjetDecorsZBuf() ;

	// calcul les coordonnées 2D de chaque points
	ptra = &SommetAffichable[0][0];
	ptrm = MapSommetY ;
	ptrs = &SommetRot[0][0] ;

	for (S32 z=0; z<=NB_COTE*512; z+=512)
	{
		for (S32 x=0; x<=NB_COTE*512; x+=512)
		{
			S32	zr ;

			LongWorldRotatePoint(x, *ptrm, z);

			zr = CameraZr - Z0 ;// l'ecran

			// cone clipping

			if( (zr<ClipZFar)
			AND (zr>NearClip)
			AND LongProjectPoint( X0,Y0,Z0 ) )
			{
				U8	flag	;

				flag = 16	;

				if( Xp < ClipXMin )	flag |= 1 ;
				if( Xp > ClipXMax )	flag |= 2 ;
				if( Yp < ClipYMin )	flag |= 4 ;
				if( Yp > ClipYMax )	flag |= 8 ;

				*ptra		= flag 	;

				ptrs->Zrot 	= zr;

				// Add 0.5 bias before truncation to reduce edge artifacts from coordinate precision loss
				ptrs->X2D	= (S16)(Xp + 0.5);
				ptrs->Y2D	= (S16)(Yp + 0.5);

			}
			else
			{
				*ptra = 0	;
			}

			ptra++;
			ptrm++;
			ptrs++;
		}
	}

	PtrMap = GroundTexture ;
	RepMask = 0xFFFF ;

	ptra  = &SommetAffichable[0][0] ;
	ptrs  = &SommetRot[0][0] ;

	for(z=0; z<NB_COTE; z++)
	{
		for(S32 x=0; x<NB_COTE; x++)
		{
			if (MaskVisible(ptra) == 16)
			{
				DrawFeuillePolyZBuf( x, z, ptrs) ;
			}

			ptra++	;
			ptrs++	;
		}

		ptra++	;
		ptrs++	;
	}

	BoxStaticAdd( 0, 0, ModeDesiredX-1, ModeDesiredY-1 ) ;
}

*/
/*──────────────────────────────────────────────────────────────────────────*/
//****************************************************************************

void AffichageTerrainRapide(void) {
    for (S32 xi = 0; xi <= NB_COTE; xi++) {
        for (S32 zi = 0; zi <= NB_COTE; zi++) {
            LongWorldRotatePoint(xi * 512,
                                 (S32)MapSommetY[zi * 65 + xi],
                                 zi * 512);

            if (LongProjectPoint(X0, Y0, Z0)) {
                Plot(Xp, Yp, WHITE);
            }
        }
    }
}

/*──────────────────────────────────────────────────────────────────────────*/
/*──────────────────────────────────────────────────────────────────────────*/

void CopyBlock256(U8 *sour, U8 *dest) {
    S32 y;

    for (y = 0; y < 32; y++) {
        memcpy(dest, sour, 32);
        dest += 256;
        sour += 256;
    }
}

/*──────────────────────────────────────────────────────────────────────────*/
S32 RefreshAniPoly = TRUE;
S32 NbToRefresh = 0;

/*──────────────────────────────────────────────────────────────────────────*/
void ClearAniPoly(void) {
    NbAniPoly = 0;
    PtrAniPoly = PtrPolySea;
    RefreshAniPoly = NbAniPoly;
    NbToRefresh = NbAniPoly / 4;
}

/*──────────────────────────────────────────────────────────────────────────*/
void DoAnimatedPolys() {
    if (!NbAniPoly OR !MaxPolySea)
        return;

    if (TimerRefHR > TimerAniPoly) {
        CntAniPoly8++;
        CntAniPoly8 &= 7; // 0-8
        CntAniPoly6++;
        CntAniPoly6 %= 6; // 0-5

        TimerAniPoly = TimerRefHR + 1000 / 4; // voir pour nb frame/sec

        if (OffsetAniPoly1 != -1) {
            CopyBlock256(SkySeaTexture + 128 * 256 + CntAniPoly8 * 32,
                         GroundTexture + OffsetAniPoly1);
        }

        if (OffsetAniPoly2 != -1) {
            CopyBlock256(SkySeaTexture + (128 + 32) * 256 + CntAniPoly8 * 32,
                         GroundTexture + OffsetAniPoly2);
        }

        if (OffsetAniPoly3 != -1) {
            CopyBlock256(SkySeaTexture + (128 + 32 + 32) * 256 + 32 * 2 + CntAniPoly6 * 32,
                         GroundTexture + OffsetAniPoly3);
        }

        if (OffsetAniPoly4 != -1) {
            CopyBlock256(SkySeaTexture + (128 + 32 + 32 + 32) * 256 + 32 * 2 + CntAniPoly6 * 32,
                         GroundTexture + OffsetAniPoly4);
        }

        Atlas_MarkDirty(GroundTexture);
        if ((AnimateTexture || ProtectActif) && BufferTexture) {
            Atlas_MarkDirty(BufferTexture);
        }
        RefreshAniPoly = NbAniPoly;
        NbToRefresh = NbAniPoly / 4;
    }
}

/*──────────────────────────────────────────────────────────────────────────*/

void DrawAnimatedPolys() {
    S32 n;
    S_ANI_POLY *ptr;

    if (!NbAniPoly OR !MaxPolySea OR !RefreshAniPoly)
        return;

    PtrMap = GroundTexture;
    RepMask = 0xFFFF;

    Switch_Fillers(FILL_POLY_FOG_NZW);

    if (NbToRefresh > RefreshAniPoly)
        NbToRefresh = RefreshAniPoly;

    ptr = PtrAniPoly = (S_ANI_POLY *)&PtrPolySea[RefreshAniPoly - 1];
    for (n = 0; n < NbToRefresh; n++, ptr--) {
        UnsetClip();

        SetClip(ptr->X0, ptr->Y0, ptr->X1, ptr->Y1);

        if (ClipXMin <= ClipXMax
                            AND ClipYMin <= ClipYMax) {
            SetCLUT(ptr->Fog);

            Fill_Poly(ptr->Type, ptr->Color, 3, ptr->Tab_Points);

#ifdef USE_GPURENDERER
            if (!GPURendererActive)
#endif
            {
                BoxStaticAdd(ClipXMin, ClipYMin, ClipXMax, ClipYMax);
            }
        }

        RefreshAniPoly--;
    }

    // protec au cas ou
    if (RefreshAniPoly < 0)
        RefreshAniPoly = 0;

    // Met a jour Screen pour recouvrements
#ifdef USE_GPURENDERER
    if (GPURendererActive)
    {
        GL_FlushBatch();
    }
    else
#endif
    {
        BoxBlitStaticListToScreen();
    }

    Switch_Fillers(FILL_POLY_TEXTURES);
    SetCLUT(PalLevel);

    UnsetClip();
}

/*──────────────────────────────────────────────────────────────────────────*/

void FiltreAnimatedPolys() {
    S32 n;
    S_ANI_POLY *ptr;

    if (!NbAniPoly OR !MaxPolySea)
        return;

    ptr = PtrAniPoly = (S_ANI_POLY *)PtrPolySea;

    for (n = 0; n < NbAniPoly;) {
        Fill_Poly(POLY_FLAG_ZBUFFER, 0, 3, ptr->Tab_Points);

        if (IsPolygonHidden) {
            if (NbAniPoly > 1) {
                memcpy((char *)ptr, (char *)&PtrPolySea[NbAniPoly - 1], sizeof(S_ANI_POLY));
            }
            NbAniPoly--;
        } else {
            n++;
            ptr++;
        }
    }
}
