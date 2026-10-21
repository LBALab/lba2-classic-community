/*══════════════════════════════════════════════════════════════════════════*
             █   █ █▀▀▀█ █▀▀▀█       █▀▀▀▀ ▀▄ ▄▀ ▀▀█▀▀       █▀▀▀▀
             ██ ▄▀ ██▀▀█ ██▀█▀       ██▀▀   ▄▀▄    ██   ▄▄   ██
             ▀▀▀   ▀▀  ▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀   ▀   ▀▀   ▀▀   ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

#ifndef LBA_GAME
S16 FillFeuille = 1;
#endif

S32 ClipZFar;
S32 StartZFog;

//camera

S32 CamPosX;
S32 CamPosY;
S32 CamPosZ;

S32 AlphaCam = 60 * 4;
S32 BetaCam = -64 * 4;
S32 GammaCam = 0;
S32 ChampX = 600;
S32 ChampZ = 600;

S32 VueDistance = 30000;
S32 VueOffsetX = 512 * 32, VueOffsetY = 0, VueOffsetZ = 512 * 32;

S32 WorldLightAlpha;
S32 WorldLightBeta;

#ifdef LBA_EDITOR
S32 ZBuffer = FALSE;
#else
S32 ZBuffer = TRUE;
#endif

// terrain

S32 *ListCubeInfos; // size ok file stored in hqr

U8 *IsleMapIndex;

T_HALF_POLY *MapPolyGround;
T_HALF_TEX *ListTexDef;

S16 *MapSommetY;
U8 *MapIntensity;
T_SOMMET_ROT SommetRot[65][65];
U8 SommetAffichable[65][65];

S32 FogCoul;
S32 PalLevel;

U8 *SkySeaTexture;
U8 *ObjTexture;
U8 *GroundTexture;

Struc_Point Tab_Points[8];
//Struc_Point	MemoTab_Points[8] ;

// decors

S32 NbObjDecors;
T_DECORS *ListDecors;

T_HQR_HEADER *HQR_Isle_Obj;
T_HQR_HEADER *HQR_CubeInfos;
T_HQR_HEADER *HQR_ListDecors;
T_HQR_HEADER *HQR_MapPGround;
T_HQR_HEADER *HQR_ListTexDef;
T_HQR_HEADER *HQR_MapSommetY;
T_HQR_HEADER *HQR_MapIntensity;

S32 NbObjScreen = 0;
U16 CubeBitField;

T_LIST_TRI *ListTriExt;

// collision avec le terrain
S32 IndexMapPolyTested = 0; // liaison avec perso

//--------------------------------------------------------------------
// Animation de l'eau

S32 TimerAniPoly = 0;
S32 CntAniPoly8 = 0; // anim 0-7
S32 CntAniPoly6 = 0; // anim 0-5

S_ANI_POLY *PtrPolySea; // buffer stockage
S32 MaxPolySea = MAX_POLY_SEA;

S_ANI_POLY *PtrAniPoly; // pointeur poly en cours
S32 NbAniPoly = 0;

S32 OffsetAniPoly1;
S32 OffsetAniPoly2;
S32 OffsetAniPoly3;
S32 OffsetAniPoly4;

S32 AtlasTextureDirty = 0;
U8 *AtlasTextureDirtyPtrs[ATLAS_DIRTY_MAX]; /* pages mutated since the last flush */
S32 AtlasTextureDirtyCount = 0;
S32 AtlasTextureDirtyAll = 0; /* unnamed or too many pages: refresh all */

/* Name the page an in-place mutation touched.  The renderer invalidates just
   those pages; a null page or more pages than the list holds falls back to
   the conservative whole-cache refresh. */
void Atlas_MarkDirty(U8 *page) {
    S32 i;

    if (page == (U8 *)0) {
        AtlasTextureDirtyAll = 1;
        AtlasTextureDirty = 1;
        return;
    }
    if (AtlasTextureDirtyAll) {
        AtlasTextureDirty = 1;
        return;
    }
    for (i = 0; i < AtlasTextureDirtyCount; i++) {
        if (AtlasTextureDirtyPtrs[i] == page) {
            AtlasTextureDirty = 1;
            return;
        }
    }
    if (AtlasTextureDirtyCount < ATLAS_DIRTY_MAX) {
        AtlasTextureDirtyPtrs[AtlasTextureDirtyCount++] = page;
    } else {
        AtlasTextureDirtyAll = 1;
    }
    AtlasTextureDirty = 1;
}

/*──────────────────────────────────────────────────────────────────────────*/
