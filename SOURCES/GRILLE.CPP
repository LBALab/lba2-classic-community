#include "C_EXTERN.H"
#include "DIRECTORIES.H"

#include <3D/PROJREC.H>
#include <3D/CAMERA.H>
#include <SVGA/SCALESPI.H>
#include <SVGA/GRAPH.H>
#include <SVGA/SCREEN.H>

#include <SDL3/SDL.h> /* threaded xBR brick filter (SDL_CreateThread, like AIL/STREAM) */

/*══════════════════════════════════════════════════════════════════════════*
		      █▀▀▀▀ █▀▀▀█  █    █     █     █▀▀▀▀
		      ██ ▀█ ██▀█▀  ██   ██    ██    ██▀▀
		      ▀▀▀▀▀ ▀▀  ▀  ▀▀   ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

#define HEADER_BLOCK 3

#define DXBLOCK 0
#define DYBLOCK 1
#define DZBLOCK 2

extern S32 XS0, YS0, XS1, YS1, XS2, YS2, XS3, YS3; // bezier.c

/*-------------------------------------------------------------------------*/

S32 XpOrgw;
S32 YpOrgw;

#define NB_COLON 256  // 28
#define MAX_BRICK 256 // 150

S16 NbBrickColon[NB_COLON];

typedef struct
{
    S16 Xm;
    S16 Ym;
    S16 Zm;
    S16 Xs; /* screen X — stored explicitly so DrawOverBrick can re-blit at the exact
              same position the original AffBrickBlock paint used. The col index
              alone is insufficient when Map2Screen's iso origin offset is not a
              multiple of 24 (col*24-24 then loses the remainder, off-by-up-to-23
              from the original XScreen — manifests as iso bricks 'jittering' under
              DrawOverBrick at non-traditional render widths). At the historical
              640x480 the offset is 288 which is 12*24 so the legacy col*24-24
              reconstruction was exact. */
    S16 Ys;
    S16 Brick;
    S16 Col;

} T_COLONB;

T_COLONB ListBrickColon[NB_COLON][MAX_BRICK];

U8 *BufferMaskBrick = NULL;
S32 NbBlock;
S32 DxBlock, DyBlock, DzBlock;

S32 ForbidenBrick = 0;

U8 CodeJeu = 0;
S32 XMap = 0, YMap = 0, ZMap = 0;
S32 XScreen = 0, YScreen = 0;
PTR_U8 BufCube = 0;
PTR_U8 BufMap = 0;
PTR_U8 TabBlock = 0;
PTR_U8 BufferBrick = 0;

extern S32 Nxw, Nyw, Nzw;

/* Iso view scale (zoom) brick blit + re-blit; defined below, forward-declared so
   the DrawOverBrick* re-blit (above their definition) can use them. */
static void BlitBrickIso(S32 numbrick, S32 xs, S32 ys);
static void ReBlitBrickIso(S32 numbrick, S32 xs, S32 ys);

//void	ReajustPos( U8 col ) ;
/*--------------------------------------------------------------------------*/
typedef struct
{
    U16 Gri_Start;                //	Index of GRI
    U16 Grm_Start;                //	Index of GRM
    U16 Bll_Start;                //	Index of BLL
    U16 Brk_Start;                //	Index of BRK
    U16 Max_Brk;                  //	Nb Brk
    U16 ForbidenBrick;            //	NEW : Brick Interdite
    U32 Max_Size_Gri;             //	Max Size Gri
    U32 Max_Size_Bll;             //	Max Size Bll
    U32 Max_Size_Brick_Cube;      //	Max Ideal Style Size
    U32 Max_Size_Mask_Brick_Cube; //	Max Size Mask

} T_BKG_HEADER;

T_BKG_HEADER BkgHeader;

typedef struct
{
    U8 My_Bll;        //	Index Style ( + Bll_Start )
    U8 My_Grm;        //	Index Grm ( + Grm_Start )
    U8 UsedBlock[32]; //	Bluck Used in Bll ( Bits )

} T_GRI_HEADER;

T_GRI_HEADER *GriHeader;
/*--------------------------------------------------------------------------*/
/*══════════════════════════════════════════════════════════════════════════*
	     █   █ █▀▀▀█ █▀▀▀█       █▀▀█  █▀▀▀█  █    █▀▀▀▀ █  ▄▀
	     ██▀▀█ ██ ▄█ ██▀█▀       ██▀▀█ ██▀█▀  ██   ██    ██▀▄
	     ▀▀  ▀ ▀▀▀▀  ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀  ▀  ▀▀   ▀▀▀▀▀ ▀▀  ▀
 *══════════════════════════════════════════════════════════════════════════*/
#define MAX_BRICK_GAME 20000L
#define OFFSET_BUFFER_FLAG 153800L

/*--------------------------------------------------------------------------*/
S32 LoadUsedBrick() {
    U32 i, j, b;
    U8 *pt;
    U8 *ptb;
    U16 *ptw;
    U32 *ptoff;
    U8 *ptdata;
    U32 offset;
    U32 maxbrk;
    U32 brick;
    U32 offseek;
    U32 *ptseek;
    U32 handle;
    U16 *tabflag;
    U16 *ptflag;
    U32 min, max;
    U8 *ptdecomp;
    COMPRESSED_HEADER header;
    S32 nbbrick;
    char tmpFilePath[ADELINE_MAX_PATH];

    tabflag = (U16 *)((U8 *)Screen + OFFSET_BUFFER_FLAG);

    memset(tabflag, 0, MAX_BRICK_GAME * 2L); /* Table de U16 pour NewNumBrick */

    min = 60000;
    max = 0;

    //-------------------------------------- Premiere Passe, Préparation ptflag

    pt = GriHeader->UsedBlock; /* Debut de Used Block */

    nbbrick = 0;

    for (i = 1; i < 256; i++) {
        b = pt[i >> 3] & (1 << (7 - (i & 7))); /*	Recup Bit Block	*/
        if (!b)
            continue;

        ptb = TabBlock + *(U32 *)(TabBlock + ((i - 1) << 2));
        ptw = (U16 *)(ptb + 5);                  /* Jump dx dy dz et collis */
        maxbrk = *ptb * *(ptb + 1) * *(ptb + 2); /* dx*dy*dz*/

        for (j = 0; j < maxbrk; j++, ptw += 2) {
            brick = *ptw;

            if (brick) {
                brick--;

                if (brick < min)
                    min = brick;
                if (brick > max)
                    max = brick;

                if (tabflag[brick] == 0) {
                    nbbrick++;
                    tabflag[brick] = 1;
                }
            }
        }
    }

    //-------------------------------------- Deuxieme Passe, Load Brick

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BKG_HQR_NAME);
    handle = OpenRead(tmpFilePath);
    if (!handle)
        return (0L);

    Seek(handle, BkgHeader.Brk_Start << 2, SEEK_FROM_START); // Begin offsets Brk

    Read(handle, Screen, BkgHeader.Max_Brk << 2); // Read Offsets Brk

    ptflag = tabflag + min;

    ptoff = (U32 *)BufferBrick;

    offset = (nbbrick + 1) * 4L;

    *ptoff++ = offset; /* First Offset        */

    ptdata = BufferBrick + offset; /* Jump nbbrick+1 Offset	*/

    nbbrick = 0;

    ptseek = (U32 *)Screen;

    for (i = min; i <= max; i++, ptflag++) {
        if (*ptflag) {
            nbbrick++;              /*	One More*/
            *ptflag = (S16)nbbrick; /*	Brick+1, for BLL */
            offseek = *(ptseek + i);
            Seek(handle, offseek, SEEK_FROM_START);
            Read(handle, &header, sizeof(header));
            switch (header.CompressMethod) {
            case 0: //	No compression
                Read(handle, ptdata, header.SizeFile);
                break;
            case 1: //	Compression LZSS
                ptdecomp = ptdata + header.SizeFile - header.CompressedSizeFile + 500;
                Read(handle, ptdecomp, header.CompressedSizeFile);
                //					ExpandLZSS( ptdecomp, ptdata, header.SizeFile )	;
                ExpandLZ(ptdata, ptdecomp, header.SizeFile, 2);
                break;
            }
            ptdata += header.SizeFile;
            offset += header.SizeFile;
            *ptoff++ = offset;
        }
    }

    //-------------------------------------- Troisieme passe, Rename Block

    pt = GriHeader->UsedBlock; /* Debut de Used Block */

    for (i = 1; i < 256; i++) {
        b = pt[i >> 3] & (1 << (7 - (i & 7))); /*	Recup Bit Block	*/
        if (!b)
            continue;

        ptb = TabBlock + *(U32 *)(TabBlock + ((i - 1) << 2));
        ptw = (U16 *)(ptb + 5);                  /* Jump dx dy dz et collis */
        maxbrk = *ptb * *(ptb + 1) * *(ptb + 2); /* dx*dy*dz*/

        for (j = 0; j < maxbrk; j++, ptw += 2) {
            if (*ptw)
                *ptw = tabflag[(*ptw) - 1];
        }
    }

    Close(handle);

    ForbidenBrick = tabflag[BkgHeader.ForbidenBrick];

    return (offset);
}
/*-------------------------------------------------------------------------*/
void InitGrille(U32 numcube) {
    U32 id_gri;
    U32 id_bll;
    char tmpFilePath[ADELINE_MAX_PATH];

    numcube = TabAllCube[numcube].Num; //	Indirection

    id_gri = BkgHeader.Gri_Start + numcube;

    GriHeader = (T_GRI_HEADER *)BufMap;

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BKG_HQR_NAME);
    Load_HQR(tmpFilePath, BufMap, id_gri);

    ManageSystem(); // for streaming

    id_bll = BkgHeader.Bll_Start + GriHeader->My_Bll; //+numcube

    Load_HQR(tmpFilePath, TabBlock, id_bll);

    //----------------------------------------------	NEW
    if (!LoadUsedBrick())
        return;
    //----------------------------------------------

    ManageSystem(); // for streaming

    CreateMaskGph(BufferBrick, BufferMaskBrick);

    ManageSystem(); // for streaming

    NbBlock = (*(U32 *)TabBlock) / 4;

    CopyMapToCube();

    ManageSystem(); // for streaming

    RedrawGRMs(); // au cas ou il y ait des GRMs actives

    return;
}

/*--------------------------------------------------------------------------*/
void FreeGrille() {}
/*-------------------------------------------------------------------------*/
/*══════════════════════════════════════════════════════════════════════════*
		   █▀▀▀▀ ██▄ █ █▀▀▀▄       █   █ █▀▀▀█ █▀▀▀█
		   ██▀▀  ██▀██ ██  █       ██▀▀█ ██ ▄█ ██▀█▀
		   ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀        ▀▀  ▀ ▀▀▀▀  ▀▀  ▀
 *══════════════════════════════════════════════════════════════════════════*/
/*--------------------------------------------------------------------------*/
/*══════════════════════════════════════════════════════════════════════════*
	     █▀▀▀▀ █   █ ██▄ █ █▀▀▀▀ ▀▀█▀▀  █    █▀▀▀█ ██▄ █ ██▀▀▀
	     ██▀▀  ██  █ ██▀██ ██      ██   ██   ██  █ ██▀██ ▀▀▀▀█
	     ▀▀    ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀   ▀▀   ▀▀   ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/*--------------------------------------------------------------------------*/
U8 *GetAdrColonneMap(S32 x, S32 z) {
    U32 offset;

    offset = *(U16 *)(BufMap + sizeof(T_GRI_HEADER) + (x + (z * SIZE_CUBE_Z)) * 2);

    return (BufMap + sizeof(T_GRI_HEADER) + offset);
}
/*--------------------------------------------------------------------------*/
U8 *GetAdrColonneCube(S32 x, S32 z) {
    U32 offset;

    offset = (x * SIZE_CUBE_Y * 2) + (z * SIZE_CUBE_Z * SIZE_CUBE_Y * 2);

    return (BufCube + offset);
}

/*--------------------------------------------------------------------------*/
U8 WorldColBrickVisible(S32 oldx, S32 oldy, S32 oldz,
                        S32 xw, S32 yw, S32 zw) {
    U8 col = GetWorldColBrickVisible(xw, yw, zw);

    if (col)
        return (col);

    if (abs(xw - oldx) >= SIZE_BRICK_XZ
                              OR abs(yw - oldy) >= SIZE_BRICK_Y
                                                       OR abs(zw - oldz) >= SIZE_BRICK_XZ) {
        return (WorldColBrickVisible(oldx, oldy, oldz,
                                     oldx + (xw - oldx) / 2,
                                     oldy + (yw - oldy) / 2,
                                     oldz + (zw - oldz) / 2));
    }

    return (0); // Ok, cette fois, on est sûr !:)
}

/*--------------------------------------------------------------------------*/
/*U8	WorldColBrickVisible( S32 xw, S32 yw, S32 zw )
{
	if( GetBlockBrick( xw, yw, zw ) )
	{
		return( WorldColBrick( xw, yw, zw ) ) ;
	}
	else	return 0 ;
}
*/

/*--------------------------------------------------------------------------*/

void GetShadow(S32 xw, S32 yw, S32 zw) {
    S32 xm, ym, zm;
    U8 *ptc;
    S32 y;
    U8 *adr;
    S32 block;

    xm = xw / SIZE_BRICK_XZ;
    ym = yw / SIZE_BRICK_Y;
    zm = zw / SIZE_BRICK_XZ;

    ShadowX = Nxw = xw;
    ShadowZ = Nzw = zw;

    if (ym >= SIZE_CUBE_Y) // au-dessus du plafond de la map
    {
        ym = SIZE_CUBE_Y - 1;
    }

    ptc = BufCube + ym * 2 + (xm * SIZE_CUBE_Y * 2) + (zm * SIZE_CUBE_X * SIZE_CUBE_Y * 2);

    for (y = ym; y > 0; y--) {
        if (*ptc != 0)
            break;
        ptc -= 2;
    }

    XMap = xm;
    YMap = y;
    ZMap = zm;

    Nyw = (y + 1) * SIZE_BRICK_Y;

    ShadowCol = 0;

    if (*ptc != 0) {
        block = (*ptc++) - 1;
        adr = GetAdrBlock(block);
        adr += HEADER_BLOCK;
        adr += (*ptc) << 2; /* 4 Bytes to Jump	*/
        ShadowCol = *adr;
        ReajustPos(ShadowCol);
    }

    ShadowY = AdjustShadowObjects(xw, zw, Nyw, yw);
}

/*──────────────────────────────────────────────────────────────────────────*/
/*--------------------------------------------------------------------------*/
S32 ProjectShadow(T_SORT *ptrs, S32 beta) {
    S32 xmin, zmin, xmax, zmax, xw, yw, zw;
    S32 ys, cy;
    S32 altitude_ref;
    S32 demix, demiz;

    ClearScreenMinMax();

    xw = ptrs->Xw;
    yw = ptrs->Yw;
    zw = ptrs->Zw;

    xmin = ptrs->XMin - xw;
    zmin = ptrs->ZMin - zw;
    xmax = ptrs->XMax - xw;
    zmax = ptrs->ZMax - zw;

    demix = (xmin + xmax) / 2;
    demiz = (zmin + zmax) / 2;

    // inclure ici la gestion des objets du decors
    altitude_ref = AdjustShadowObjects(xw, zw, yw, yw);

    Rotate(demix, zmin, beta);

    X0 += xw;
    Z0 += zw;
    ys = altitude_ref + SIZE_BRICK_Y;

    GetShadow(X0, ys, Z0);
    cy = AdjustShadowObjects(X0, Z0, ShadowY, yw);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    if (!LongProjectPoint(X0, cy, Z0))
        return FALSE;
    XS0 = Xp;
    YS0 = Yp;
    AdjustScreenMinMax();

    Rotate(xmin, demiz, beta);

    X0 += xw;
    Z0 += zw;

    GetShadow(X0, ys, Z0);
    cy = AdjustShadowObjects(X0, Z0, ShadowY, yw);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    if (!LongProjectPoint(X0, cy, Z0))
        return FALSE;
    XS1 = Xp;
    YS1 = Yp;
    AdjustScreenMinMax();

    Rotate(demix, zmax, beta);

    X0 += xw;
    Z0 += zw;

    GetShadow(X0, ys, Z0);
    cy = AdjustShadowObjects(X0, Z0, ShadowY, yw);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    if (!LongProjectPoint(X0, cy, Z0))
        return FALSE;
    XS2 = Xp;
    YS2 = Yp;
    AdjustScreenMinMax();

    Rotate(xmax, demiz, beta);

    X0 += xw;
    Z0 += zw;

    GetShadow(X0, ys, Z0);
    cy = AdjustShadowObjects(X0, Z0, ShadowY, yw);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    if (!LongProjectPoint(X0, cy, Z0))
        return FALSE;
    XS3 = Xp;
    YS3 = Yp;
    AdjustScreenMinMax();

    UnsetClip();
    SetClip(ScreenXMin, ScreenYMin, ScreenXMax, ScreenYMax);

    return TRUE;
}

/*══════════════════════════════════════════════════════════════════════════*
		   █▄ ▄█ █▀▀▀▀ █▄ ▄█ █▀▀▀█  █    █▀▀▀█ █▀▀▀▀
		   ██▀ █ ██▀▀  ██▀ █ ██  █  ██   ██▀█▀ ██▀▀
		   ▀▀  ▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀  ▀▀   ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

U32 CreateMaskGph(U8 *ptsrc, U8 *ptdst) {
    U8 *ptd;
    U32 nbg, off;
    U32 *ptoff;
    U32 size, i;

    ptoff = (U32 *)ptdst;

    off = *(U32 *)ptsrc; /*	First Offset Src	*/

    *ptoff++ = off; /*	First Offset	*/

    ptd = ptdst + off;

    nbg = (off - 4) >> 2; /*	Nombre de Graph	*/

    for (i = 0; i < nbg; i++) {
        size = CalcGraphMsk(i, ptsrc, ptd);

        off += size;    /*	Maj Offset	*/
        *ptoff++ = off; /*	Write Offset 	*/
        ptd += size;    /*	Maj Pt Dest	*/
    }
    return (off);
}

/*-------------------------------------------------------------------------*/
void CopyMapToCube() {
    U8 *pts, *ptd;
    S32 x, z;

    /*-------------- Copy -------------------------*/
    for (z = 0; z < SIZE_CUBE_Z; z++) {
        for (x = 0; x < SIZE_CUBE_X; x++) {
            pts = GetAdrColonneMap(x, z);
            ptd = GetAdrColonneCube(x, z);

            DecompColonne(pts, ptd);
        }
    }
}
/*──────────────────────────────────────────────────────────────────────────*/
void InitBufferCube() {
    S32 intmem;
    char tmpFilePath[ADELINE_MAX_PATH];

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BKG_HQR_NAME);

    //	pas de RECOVER_AREA car BkgHeader en stored et pas en LZSS
    Load_HQR(tmpFilePath, &BkgHeader, 0); // HEADER

    // 	pas de RECOVER_AREA car TabAllCube en stored et pas en LZSS
    Load_HQR(tmpFilePath, (void *)TabAllCube, BkgHeader.Brk_Start + BkgHeader.Max_Brk);

    // 	Calcul des tailles nécessaires
    intmem = (BkgHeader.Max_Size_Brick_Cube + RECOVER_AREA)    // BufferBrick
             + (SIZE_CUBE_X * SIZE_CUBE_Y * SIZE_CUBE_Z * 2L); // BufCube

    //	PtrZBuffer sert aussi pour les décors intérieurs. The Z-buffer floor
    //	is set by Mem_ConfigureScreenBuffers() before InitMainBuffer runs (the
    //	SIZE_Z_BUFFER macro needs ModeDesiredX which isn't valid yet here);
    //	only grow above that floor if the cube's brick + cube data needs more.
    if (intmem > ListMem[MEM_PTR_ZBUFFER].Size)
        ListMem[MEM_PTR_ZBUFFER].Size = intmem;

    ListMem[MEM_BUFFER_MASK_BRICK].Size = BkgHeader.Max_Size_Mask_Brick_Cube;
    ListMem[MEM_BUF_MAP].Size = BkgHeader.Max_Size_Gri + RECOVER_AREA;
    ListMem[MEM_TAB_BLOCK].Size = BkgHeader.Max_Size_Bll + RECOVER_AREA;
}

/*══════════════════════════════════════════════════════════════════════════*
		   █▀▀▀▄  █    ██▀▀▀ █▀▀▀█ █     █▀▀▀█ █  ▄▀
		   ██  █  ██   ▀▀▀▀█ ██▀▀▀ ██    ██▀▀█ ██▀
		   ▀▀▀▀   ▀▀   ▀▀▀▀▀ ▀▀    ▀▀▀▀▀ ▀▀  ▀ ▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/* Does a stored foreground brick vertically overlap the current object clip
   window?  At ISO_SCALE_ONE this is the original `Ys + 38 > ClipYMin && Ys <=
   ClipYMax` (bit-exact: top=Ys, bottom=Ys+38). Under zoom the single 38px height
   approximation under-reaches for tall/spread wall bricks, so foreground bricks
   fail the test and never re-blit over the object (it shows through the wall).
   Use the brick's actual scaled extent instead: top = Ys + HotY*scale,
   bottom = top + DeltaY*scale, from the brick header. */
static int BrickOverlapsClipY(const T_COLONB *b) {
    S32 top, bot;
    if (IsoScale == ISO_SCALE_ONE) {
        top = b->Ys;
        bot = b->Ys + 38;
    } else {
        U8 *data = (U8 *)BufferBrick + ((U32 *)BufferBrick)[b->Brick];
        top = b->Ys + ISO_PX((S8)data[3]); /* HotY (signed) */
        bot = top + ISO_PX(data[1]);       /* DeltaY */
    }
    return bot > ClipYMin AND top <= ClipYMax;
}

void DrawOverBrick(S32 xm, S32 ym, S32 zm) {
    register T_COLONB *ptrlbc;
    S32 col, i;
    S32 startcol, endcol;
    S32 xw, yw, zw;
    S32 recouvre;

    // Bias -2, not -1: a wall one iso-cube south of the actor projects to
    // XScreen = actor_iso_XScreen - 24, i.e. col = actor_iso_col - 1. The
    // historical -1 only catches that wall when the actor's iso center happens
    // to land on a col-bucket boundary — true at widths where
    // (ModeDesiredX/2 - 32) % 24 == 0 (640 / 1024 / 1600). At 768 (offset
    // 352 = 14*24 + 16) the actor's iso center lands inside a col bucket and
    // the clip-derived `col(ClipXMin) - 1` sits in the *same* col as the
    // actor, missing the south-wall col entirely — DrawOverBrick never re-blits
    // those walls and they appear under the actor. -2 reliably covers one
    // extra col regardless of alignment. The extra col is a free harmless
    // scan when no walls live in it; when walls do live in it but don't
    // overlap the actor, CopyMask paints offscreen of the actor (same
    // wasted-CPU pattern the 640 build was already doing). See
    // WIDESCREEN_HARDCODED_DIMS_AUDIT.md.
    // Iso zoom: scale the column granularity with the view. ISO_PX(24) is the
    // scaled half-brick column width, so the native -2 back-scan (one full brick
    // of columns) stays correct at any scale. ISO_PX is identity at 1.0.
    S32 isoCol = ISO_PX(24);
    startcol = (ClipXMin + isoCol) / isoCol - 2;
    if (startcol < 0)
        startcol = 0;
    endcol = (ClipXMax + isoCol) / isoCol;

    for (col = startcol; col <= endcol; col++) {
        ptrlbc = &ListBrickColon[col][0];

        for (i = 0; i < NbBrickColon[col]; i++) {
            /* bricks devant */
            if (BrickOverlapsClipY(ptrlbc)) {
                recouvre = FALSE;

                // essai correction bugs escaliers
                //			if( WorldColBrick(ptrlbc->Xm*SIZE_BRICK_XZ,ptrlbc->Ym*SIZE_BRICK_Y,ptrlbc->Zm*SIZE_BRICK_XZ) > 1 )
                if (ptrlbc->Col > 1) // collis complexe ?
                {
                    if (ptrlbc->Ym >= ym + 1) {
                        recouvre = TRUE;
                    }
                } else if (ptrlbc->Ym >= ym) {
                    recouvre = TRUE;
                }

                if (recouvre) {
                    xw = ptrlbc->Xm * SIZE_BRICK_XZ;
                    yw = ptrlbc->Ym * SIZE_BRICK_Y;
                    zw = ptrlbc->Zm * SIZE_BRICK_XZ;

                    if ((ptrlbc->Xm >= xm AND ptrlbc->Zm >= zm)
                            OR(ptrlbc->Xm > xm
                                                AND WorldColBrick(xw - SIZE_BRICK_XZ, yw, zw) !=
                               1
                                   //				AND WorldColBrick(xw,yw,zw+SIZE_BRICK_XZ)==1 )
                                   AND WorldColBrick(xw, yw, zw + SIZE_BRICK_XZ) >= 1)
                                OR(ptrlbc->Zm > zm
                                                    AND WorldColBrick(xw, yw, zw - SIZE_BRICK_XZ) !=
                                   1
                                       //				AND WorldColBrick(xw+SIZE_BRICK_XZ,yw,zw)==1 ) )
                                       AND WorldColBrick(xw + SIZE_BRICK_XZ, yw, zw) >= 1)) {
                        ReBlitBrickIso(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys);
                    }
                }
            }
            ptrlbc++;
        }
    }
}

/*--------------------------------------------------------------------------*/
/* recouvrement reel pour obj qui ne depassent pas la ZV */

void DrawOverBrick3(S32 xm, S32 ym, S32 zm, S32 ymax) {
    register T_COLONB *ptrlbc;
    S32 col, i;
    S32 startcol, endcol;

    S32 isoCol = ISO_PX(24); // scaled half-brick column width
    startcol = (ClipXMin + isoCol) / isoCol - 1;
    endcol = (ClipXMax + isoCol) / isoCol;

    for (col = startcol; col <= endcol; col++) {
        ptrlbc = &ListBrickColon[col][0];

        for (i = 0; i < NbBrickColon[col]; i++, ptrlbc++) {
            /* bricks devant */
            if (BrickOverlapsClipY(ptrlbc)) {
                if (ptrlbc->Ym > ymax) {
                    if ((ptrlbc->Zm >= zm)
                            OR(ptrlbc->Xm >= xm)) {
                        ReBlitBrickIso(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys);
                    }
                } else if (ptrlbc->Ym >= ym) {
                    if ((ptrlbc->Zm == zm)
                            AND(ptrlbc->Xm == xm)) {
                        ReBlitBrickIso(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys);
                    }

                    if ((ptrlbc->Zm > zm)
                            OR(ptrlbc->Xm > xm)) {
                        ReBlitBrickIso(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys);
                    }
                }
            }
        }
    }
}

/*--------------------------------------------------------------------------*/
// recouvrement reel pour obj qui ne depassent pas la ZV
// GROSSE RUSTINE: Cage à la fin de LBA2 pour ne pas recouvrir l'objet avec
// les briques du sol en 0

void DrawOverBrickCage(S32 xm, S32 ym, S32 zm) {
    register T_COLONB *ptrlbc;
    S32 col, i;
    S32 startcol, endcol;

    S32 isoCol = ISO_PX(24); // scaled half-brick column width
    startcol = (ClipXMin + isoCol) / isoCol - 1;
    endcol = (ClipXMax + isoCol) / isoCol;

    for (col = startcol; col <= endcol; col++) {
        ptrlbc = &ListBrickColon[col][0];

        for (i = 0; i < NbBrickColon[col]; i++, ptrlbc++) {
            /* bricks devant */
            if (BrickOverlapsClipY(ptrlbc)) {
                if (ptrlbc->Ym > 0 // diff ici avec DrawOverBrick3
                                 AND ptrlbc->Ym >= ym) {
                    if ((ptrlbc->Zm == zm)
                            AND(ptrlbc->Xm == xm)) {
                        ReBlitBrickIso(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys);
                    }

                    if ((ptrlbc->Zm > zm)
                            OR(ptrlbc->Xm > xm)) {
                        ReBlitBrickIso(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys);
                    }
                }
            }
        }
    }
}

/*--------------------------------------------------------------------------*/
/* Iso view scale (zoom) brick blit.  Bricks are RLE-compressed tiles drawn at
   native size by AffGraph; under IsoScale > 1 the grid spacing (Map2Screen)
   grows but the tiles would not, leaving gaps.  When the scale is not 1.0 we
   decode the brick RLE into a raw Struc_Sprite buffer and let ScaleSprite (the
   engine's tested scaled-sprite blitter, which needs raw pixels) draw it at the
   matching factor.  At ISO_SCALE_ONE we stay on AffGraph, bit-for-bit. */

/* offset table (4) + Struc_Sprite header (4) + raw pixels (deltaX*deltaY, both
   U8 so <= 255*255). */
static U8 s_brickRawBank[8 + 256 * 256];
/* EXPERIMENT (feat/iso-xbr-bricks): composed-background brick filtering.
   s_brickScratch = bricks composed at native scale (half the 2x view), framing the
   same region; s_brickRgb = that, edge-filtered to 2x in RGB and aligned to
   Log/Screen. RGB (not 8-bit) because an interpolating filter makes new colours
   the palette can't hold, so the composite happens at present time. */
static U8 *s_brickScratch = NULL;
static U32 *s_brickRgb = NULL;
static S32 s_epxW = 0, s_epxH = 0;
static U32 s_palLut[256]; /* current palette as ARGB (rebuilt each BuildXbrBrickBg) */
S32 BrickEpxOn = 0;       /* console toggle (iso epx) */

/* Decode one AffGraph-format RLE image (skip/copy/repeat) from any sprite bank
   into s_brickRawBank as raw pixels (colour 0 = transparent), so it can be fed to
   ScaleSprite. The bank layout is a U32 offset table followed by per-image data
   {deltaX, deltaY, hotX, hotY, rows...}, shared by bricks (BufferBrick) and the
   anim-3DS sprite objects (doors etc.), both of which AffGraph draws. */
static void DecodeRLEToRaw(const U8 *buf, S32 index) {
    const U8 *data = buf + ((const U32 *)buf)[index];
    U8 deltaX = data[0];
    U8 deltaY = data[1];
    const U8 *g = data + 4;

    ((U32 *)s_brickRawBank)[0] = 4; /* sprite 0 starts right after the offset */
    s_brickRawBank[4] = deltaX;
    s_brickRawBank[5] = deltaY;
    s_brickRawBank[6] = data[2]; /* hotX */
    s_brickRawBank[7] = data[3]; /* hotY */

    U8 *raw = s_brickRawBank + 8;
    memset(raw, 0, (size_t)deltaX * deltaY); /* colour 0 = transparent */

    for (S32 line = 0; line < deltaY; line++) {
        U8 nbBlock = *g++;
        U8 *row = raw + (S32)line * deltaX;
        S32 xpix = 0;
        for (S32 b = 0; b < nbBlock; b++) {
            U8 control = *g;
            U8 opCode = control >> 6;
            S32 counter = (control & 0x3F) + 1; /* all runs are +1, as in AffGraph */
            if (opCode == 0) {                  /* skip (transparent) */
                g++;
                xpix += counter;
            } else if (opCode == 1) { /* copy literal pixels */
                g++;
                while (counter-- > 0)
                    row[xpix++] = *g++;
            } else { /* repeat one colour */
                U8 color = g[1];
                g += 2;
                while (counter-- > 0)
                    row[xpix++] = color;
            }
        }
    }
}

static void DecodeBrickToRaw(S32 numbrick) {
    DecodeRLEToRaw((const U8 *)BufferBrick, numbrick);
}

/* Draw an anim-3DS sprite image (a SPRITE_3D object such as a door) scaled to the
   iso view. At ISO_SCALE_ONE this is the original native AffGraph blit, bit-exact.
   Under zoom the RLE image is decoded to raw and stretched with ScaleSprite so the
   sprite tracks the scaled scene instead of staying native-size (audit group D). */
void BlitAnim3DSIso(U8 *buf, S32 index, S32 xs, S32 ys) {
    if (IsoScale == ISO_SCALE_ONE) {
        AffGraph(index, xs, ys, buf);
    } else {
        DecodeRLEToRaw(buf, index);
        /* ScaleSprite factors are 16.16; IsoScale is 8.8 (ISO_SCALE_ONE=256). */
        ScaleSprite(0, xs, ys, IsoScale << 8, IsoScale << 8, s_brickRawBank);
    }
}

/* Average two ARGB pixels (fast, no overflow). */
static inline U32 BlendArgb(U32 a, U32 b) {
    return (((a ^ b) & 0xFEFEFEFEu) >> 1) + (a & b);
}

/* EXPERIMENT (feat/iso-xbr-bricks): EPX/Scale2x edge rules, but at a detected
   edge the output subpixel is the *blend* of the centre and the neighbour rather
   than a hard selection. That anti-aliases the diagonals (real RGB output, new
   colours), which is the point of going to the RGB present path. The edge test
   still compares palette indices (equal index == equal colour). Placeholder for
   real xBR; pluggable. src = 8-bit indices; dst = ARGB at 2x; pal = index->ARGB. */
static void EpxBlendScale2x(const U8 *src, S32 w, S32 h, U32 *dst, const U32 *pal) {
    S32 dw = 2 * w;
    for (S32 y = 0; y < h; y++) {
        for (S32 x = 0; x < w; x++) {
            U8 P = src[y * w + x];
            U8 A = (y > 0) ? src[(y - 1) * w + x] : P;     /* up */
            U8 B = (x < w - 1) ? src[y * w + x + 1] : P;   /* right */
            U8 C = (x > 0) ? src[y * w + x - 1] : P;       /* left */
            U8 D = (y < h - 1) ? src[(y + 1) * w + x] : P; /* down */
            U32 pP = pal[P];
            U32 e0 = pP, e1 = pP, e2 = pP, e3 = pP;
            if (C == A && C != D && A != B)
                e0 = BlendArgb(pP, pal[A]);
            if (A == B && A != C && B != D)
                e1 = BlendArgb(pP, pal[B]);
            if (D == C && D != B && C != A)
                e2 = BlendArgb(pP, pal[C]);
            if (B == D && B != A && D != C)
                e3 = BlendArgb(pP, pal[D]);
            dst[(2 * y) * dw + 2 * x] = e0;
            dst[(2 * y) * dw + 2 * x + 1] = e1;
            dst[(2 * y + 1) * dw + 2 * x] = e2;
            dst[(2 * y + 1) * dw + 2 * x + 1] = e3;
        }
    }
}

/* xBR edge metric. Luma-only weighted distance: the bricks are low-chroma, so
   dropping U/V costs ~nothing visually but cuts this hot path (called ~40x/pixel)
   to a third. The comparisons are relative, so the absolute scale is irrelevant. */
static inline int XbrWd(U32 a, U32 b) {
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF) - (int)(b & 0xFF);
    int Y = 299 * dr + 587 * dg + 114 * db;
    return Y < 0 ? -Y : Y;
}

/* Mix two ARGB by integer weights wa:wb. */
static inline U32 XbrMix(U32 a, U32 b, int wa, int wb) {
    int t = wa + wb;
    int r = ((int)((a >> 16) & 0xFF) * wa + (int)((b >> 16) & 0xFF) * wb) / t;
    int g = ((int)((a >> 8) & 0xFF) * wa + (int)((b >> 8) & 0xFF) * wb) / t;
    int bl = ((int)(a & 0xFF) * wa + (int)(b & 0xFF) * wb) / t;
    return 0xFF000000u | ((U32)r << 16) | ((U32)g << 8) | (U32)bl;
}

static inline void XbrRot90(U32 p[5][5]) {
    U32 t[5][5];
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 5; j++)
            t[i][j] = p[4 - j][i];
    memcpy(p, t, sizeof(t));
}

/* EXPERIMENT (feat/iso-xbr-bricks): xBR-style 2x. For each of the 4 output
   corners (via 4x 90-degree rotation of the neighbourhood), a 5x5 weighted edge
   detection decides whether a diagonal line cuts the corner; if so the corner
   subpixel blends toward the line colour, anti-aliasing the staircase. The 5x5
   context (vs EPX's 3x3) suppresses over-filling on noisy textures. Written from
   the published xBR edge metric, not ported from any implementation. */
static void XbrCornersFromP(U32 p[5][5], U32 out[2][2]) {
    for (int rot = 0; rot < 4; rot++) {
        U32 E = p[2][2], F = p[2][3], H = p[3][2], I = p[3][3];
        U32 C = p[1][3], G = p[3][1], B = p[1][2], D = p[2][1];
        U32 F4 = p[2][4], H5 = p[4][2], I4 = p[3][4], I5 = p[4][3];
        int left = XbrWd(E, C) + XbrWd(E, G) + XbrWd(I, F4) + XbrWd(I, H5) + 4 * XbrWd(F, H);
        int right = XbrWd(F, B) + XbrWd(F, I4) + XbrWd(H, D) + XbrWd(H, I5) + 4 * XbrWd(E, I);
        U32 res = E;
        if (left < right) {
            U32 px = (XbrWd(E, F) <= XbrWd(E, H)) ? F : H;
            res = XbrMix(px, E, 3, 1); /* corner mostly the line colour, slight AA */
        }
        if (rot == 0)
            out[1][1] = res; /* SE */
        else if (rot == 1)
            out[1][0] = res; /* SW */
        else if (rot == 2)
            out[0][0] = res; /* NW */
        else
            out[0][1] = res; /* NE */
        if (rot < 3)
            XbrRot90(p);
    }
}

/* xBR 2x over a row range [y0,y1). 8-bit source (pal != NULL) or RGB source. */
static void XbrRows(const U8 *src8, const U32 *srcRgb, S32 w, S32 h, U32 *dst, const U32 *pal, S32 y0,
                    S32 y1) {
    S32 dw = 2 * w;
    for (S32 y = y0; y < y1; y++) {
        for (S32 x = 0; x < w; x++) {
            U32 p[5][5];
            for (int dy = -2; dy <= 2; dy++) {
                int sy = y + dy;
                sy = sy < 0 ? 0 : (sy >= h ? h - 1 : sy);
                for (int dx = -2; dx <= 2; dx++) {
                    int sx = x + dx;
                    sx = sx < 0 ? 0 : (sx >= w ? w - 1 : sx);
                    p[dy + 2][dx + 2] = src8 ? pal[src8[sy * w + sx]] : srcRgb[sy * w + sx];
                }
            }
            U32 out[2][2];
            XbrCornersFromP(p, out);
            dst[(2 * y) * dw + 2 * x] = out[0][0];
            dst[(2 * y) * dw + 2 * x + 1] = out[0][1];
            dst[(2 * y + 1) * dw + 2 * x] = out[1][0];
            dst[(2 * y + 1) * dw + 2 * x + 1] = out[1][1];
        }
    }
}

/* Split the xBR pass across CPU cores: each thread owns a disjoint band of output
   rows (read-only source, disjoint writes -> no locks). Mirrors the SDL thread use
   in AIL/SDL/STREAM. Rebuilds are gated, so per-call thread spawn is negligible. */
typedef struct {
    const U8 *src8;
    const U32 *srcRgb;
    S32 w, h;
    U32 *dst;
    const U32 *pal;
    S32 y0, y1;
} XbrJob;

static int SDLCALL XbrJobFn(void *data) {
    XbrJob *j = (XbrJob *)data;
    XbrRows(j->src8, j->srcRgb, j->w, j->h, j->dst, j->pal, j->y0, j->y1);
    return 0;
}

#define XBR_MAX_THREADS 8
static void XbrRunParallel(XbrJob proto, S32 h) {
    int n = SDL_GetNumLogicalCPUCores();
    if (n < 1)
        n = 1;
    if (n > XBR_MAX_THREADS)
        n = XBR_MAX_THREADS;
    if (n > h)
        n = h;
    XbrJob jobs[XBR_MAX_THREADS];
    SDL_Thread *th[XBR_MAX_THREADS];
    S32 chunk = (h + n - 1) / n;
    int nt = 0;
    for (int t = 0; t < n; t++) {
        S32 y0 = (S32)t * chunk, y1 = y0 + chunk;
        if (y1 > h)
            y1 = h;
        if (y0 >= y1)
            break;
        jobs[nt] = proto;
        jobs[nt].y0 = y0;
        jobs[nt].y1 = y1;
        th[nt] = SDL_CreateThread(XbrJobFn, "xbr", &jobs[nt]);
        if (!th[nt])
            XbrJobFn(&jobs[nt]); /* spawn failed: run this band inline */
        nt++;
    }
    for (int t = 0; t < nt; t++)
        if (th[t])
            SDL_WaitThread(th[t], NULL);
}

static void XbrScale2x(const U8 *src, S32 w, S32 h, U32 *dst, const U32 *pal) {
    XbrJob p = {src, NULL, w, h, dst, pal, 0, h};
    XbrRunParallel(p, h);
}

/* RGB-input xBR 2x (same core), used for the second supersample pass. */
static void XbrScale2xRgb(const U32 *src, S32 w, S32 h, U32 *dst) {
    XbrJob p = {NULL, src, w, h, dst, NULL, 0, h};
    XbrRunParallel(p, h);
}

/* Box-downscale 2x: average each 2x2 of an ARGB image (sw x sh) into (sw/2 x sh/2). */
static void BoxDown2x(const U32 *src, S32 sw, S32 sh, U32 *dst) {
    S32 dw = sw / 2, dh = sh / 2;
    for (S32 j = 0; j < dh; j++) {
        const U32 *r0 = src + (size_t)(2 * j) * sw;
        const U32 *r1 = src + (size_t)(2 * j + 1) * sw;
        for (S32 i = 0; i < dw; i++) {
            U32 a = r0[2 * i], b = r0[2 * i + 1], c = r1[2 * i], d = r1[2 * i + 1];
            int rr = (((a >> 16) & 0xFF) + ((b >> 16) & 0xFF) + ((c >> 16) & 0xFF) + ((d >> 16) & 0xFF)) / 4;
            int gg = (((a >> 8) & 0xFF) + ((b >> 8) & 0xFF) + ((c >> 8) & 0xFF) + ((d >> 8) & 0xFF)) / 4;
            int bb = ((a & 0xFF) + (b & 0xFF) + (c & 0xFF) + (d & 0xFF)) / 4;
            dst[(size_t)j * dw + i] = 0xFF000000u | ((U32)rr << 16) | ((U32)gg << 8) | (U32)bb;
        }
    }
}

static void BlitBrickIso(S32 numbrick, S32 xs, S32 ys) {
    if (IsoScale == ISO_SCALE_ONE) {
        AffGraph(numbrick, xs, ys, BufferBrick);
    } else {
        DecodeBrickToRaw(numbrick);
        /* ScaleSprite factors are 16.16; IsoScale is 8.8 (ISO_SCALE_ONE=256).
           This nearest stretch also serves as the brick mask source for the
           composed-background EPX path (see EpxCompositeToLog). */
        ScaleSprite(0, xs, ys, IsoScale << 8, IsoScale << 8, s_brickRawBank);
    }
}

/* EXPERIMENT: build the seam-free 2x RGB brick layer. Instead of a second
   AffGrille pass (which corrupts shared render state -> moving objects render
   black), recover the native composed bricks by 2x-downscaling the cached scale-2
   brick background (Screen is the nearest-doubled native bricks, so Screen[2i,2j]
   == native[i,j]), then EPX-blend that *composed* image back to 2x (no per-tile
   transparent borders -> no seams). Aligned to Screen/Log, so no offset. 2x only. */
void BuildEpxBrickBg(void) {
    if (IsoScale != 2 * ISO_SCALE_ONE)
        return;
    S32 W = (S32)ModeDesiredX, H = (S32)ModeDesiredY;
    S32 nw = W / 2, nh = H / 2;
    if (s_epxW != W || s_epxH != H) {
        free(s_brickScratch);
        free(s_brickRgb);
        s_brickScratch = (U8 *)malloc((size_t)nw * nh);
        s_brickRgb = (U32 *)malloc((size_t)W * H * sizeof(U32));
        s_epxW = W;
        s_epxH = H;
    }
    if (!s_brickScratch || !s_brickRgb)
        return;

    /* The layer is a pure function of Screen + dims + filter mode; skip the rebuild
       (and its filter cost) when none changed. The hash is ~0.1% of the filter. */
    {
        static U64 s_lastHash = 0;
        static bool s_have = false;
        const U8 *sc = (const U8 *)Screen;
        U64 hh = 1469598103934665603ULL;
        hh = (hh ^ (U64)(U32)W) * 1099511628211ULL;
        hh = (hh ^ (U64)(U32)H) * 1099511628211ULL;
        hh = (hh ^ (U64)(U32)BrickEpxOn) * 1099511628211ULL;
        size_t n = (size_t)W * H;
        for (size_t k = 0; k < n; k++)
            hh = (hh ^ sc[k]) * 1099511628211ULL;
        if (s_have && hh == s_lastHash)
            return;
        s_lastHash = hh;
        s_have = true;
    }

    /* 2x-downscale the cached brick background to recover the native composed bricks */
    U8 *S = (U8 *)Screen;
    for (S32 j = 0; j < nh; j++) {
        U32 srow = TabOffLine[2 * j];
        U8 *drow = s_brickScratch + (size_t)j * nw;
        for (S32 i = 0; i < nw; i++)
            drow[i] = S[srow + 2 * i];
    }

    /* current palette -> ARGB, then edge-filter the composed bricks to 2x RGB.
       BrickEpxOn: 1 = EPX-blend (cheap), 2 = xBR, 3 = xBR supersampled (SSAA). */
    for (S32 i = 0; i < 256; i++)
        s_palLut[i] =
            ((U32)0xFF << 24) | ((U32)PtrPal[i * 3] << 16) | ((U32)PtrPal[i * 3 + 1] << 8) | (U32)PtrPal[i * 3 + 2];
    if (BrickEpxOn == 3) {
        /* SSAA: native -> xBR 2x -> xBR 4x -> box-average down to 2x. The extra
           sub-pixel resolution lets the shallow 2:1 iso diagonals resolve smoothly. */
        static U32 *s_buf2x = NULL, *s_buf4x = NULL;
        static S32 s_ssW = 0, s_ssH = 0;
        if (s_ssW != W || s_ssH != H) {
            free(s_buf2x);
            free(s_buf4x);
            s_buf2x = (U32 *)malloc((size_t)W * H * sizeof(U32));
            s_buf4x = (U32 *)malloc((size_t)(2 * W) * (2 * H) * sizeof(U32));
            s_ssW = W;
            s_ssH = H;
        }
        if (s_buf2x && s_buf4x) {
            XbrScale2x(s_brickScratch, nw, nh, s_buf2x, s_palLut); /* 8-bit -> 2x RGB (WxH) */
            XbrScale2xRgb(s_buf2x, W, H, s_buf4x);                 /* 2x -> 4x RGB (2Wx2H) */
            BoxDown2x(s_buf4x, 2 * W, 2 * H, s_brickRgb);          /* 4x -> 2x averaged */
        } else {
            XbrScale2x(s_brickScratch, nw, nh, s_brickRgb, s_palLut); /* OOM fallback */
        }
    } else if (BrickEpxOn == 2) {
        XbrScale2x(s_brickScratch, nw, nh, s_brickRgb, s_palLut);
    } else {
        EpxBlendScale2x(s_brickScratch, nw, nh, s_brickRgb, s_palLut);
    }
}

/* Scene-present callback (runs after Log->ARGB, before the UI overlays): paint the
   finished frame from the iso brick layer + the game palette. Brick pixels (where
   Log==Screen, the nearest brick draw with no model on top) take the RGB-filtered
   composed background; model pixels are re-converted from Log through PtrPal (the
   SDL paletteLUT misses the 216-255 range some objects use, so they would otherwise
   present black). The layer is aligned to Screen/Log, so there is no offset. */
void IsoXbrPresent(U32 *frame, U32 pitch, U32 width, U32 height, const U8 *pal_rgb) {
    (void)pal_rgb;
    if (!BrickEpxOn || IsoScale != 2 * ISO_SCALE_ONE || !s_brickRgb)
        return;
    S32 W = (S32)ModeDesiredX, H = (S32)ModeDesiredY;
    if ((U32)W != width || (U32)H != height)
        return; /* present buffer size mismatch; skip */
    U8 *L = (U8 *)Log, *S = (U8 *)Screen;
    for (S32 y = 0; y < H; y++) {
        U32 lrow = TabOffLine[y];
        U32 erow = (U32)y * (U32)W;
        U32 *frow = (U32 *)((U8 *)frame + (U32)y * pitch);
        for (S32 x = 0; x < W; x++) {
            U32 idx = lrow + (U32)x;
            if (L[idx] != S[idx])
                frow[x] = s_palLut[L[idx]]; /* model: re-convert via PtrPal (the SDL
                                              paletteLUT misses the 216-255 range, so
                                              objects using it would present black) */
            else
                frow[x] = s_brickRgb[erow + (U32)x]; /* brick: filtered RGB (aligned) */
        }
    }
}

/* Re-blit a foreground brick over a moving object.  At ISO_SCALE_ONE the
   original CopyMask restores the brick pixels from the (clean) Screen copy into
   Log, bit-for-bit.  Under zoom that mask blit would paint the tile at native
   size; instead re-draw the scaled tile with BlitBrickIso, the same path the
   initial draw uses, so the foreground brick lands exactly where it was drawn.
   ScaleSprite skips colour-0 pixels, giving the same transparent-edge result as
   the mask. */
/* Scaled re-blit of a foreground brick over an object. Mirrors CopyMask: copy
   the *composited* Screen background (where overlapping bricks already resolved
   front-to-back) into Log, masked by the brick silhouette. Re-drawing the single
   raw brick (BlitBrickIso) instead loses that compositing and paints the wrong
   tile where bricks overlap. The decoded brick's non-zero pixels are the
   silhouette; Screen already holds the brick at the scaled position, so the copy
   is 1:1 at the scaled footprint. */
static void ScaledReBlitFromScreen(S32 numbrick, S32 xs, S32 ys) {
    DecodeBrickToRaw(numbrick);
    U8 deltaX = s_brickRawBank[4];
    U8 deltaY = s_brickRawBank[5];
    S32 left = xs + ISO_PX((S8)s_brickRawBank[6]); /* HotX */
    S32 top = ys + ISO_PX((S8)s_brickRawBank[7]);  /* HotY */
    S32 sW = ISO_PX(deltaX);
    S32 sH = ISO_PX(deltaY);
    U8 *raw = s_brickRawBank + 8;
    U8 *L = (U8 *)Log;
    U8 *S = (U8 *)Screen;
    for (S32 dy = 0; dy < sH; dy++) {
        S32 py = top + dy;
        if (py < ClipYMin || py > ClipYMax)
            continue;
        S32 sr = ((S32)dy * ISO_SCALE_ONE) / IsoScale;
        if (sr >= deltaY)
            sr = deltaY - 1;
        U8 *rrow = raw + sr * deltaX;
        S32 base = TabOffLine[py];
        for (S32 dx = 0; dx < sW; dx++) {
            S32 px = left + dx;
            if (px < ClipXMin || px > ClipXMax)
                continue;
            S32 sc = ((S32)dx * ISO_SCALE_ONE) / IsoScale;
            if (sc >= deltaX)
                sc = deltaX - 1;
            if (rrow[sc]) /* brick silhouette */
                L[base + px] = S[base + px];
        }
    }
}

static void ReBlitBrickIso(S32 numbrick, S32 xs, S32 ys) {
    if (IsoScale == ISO_SCALE_ONE) {
        CopyMask(numbrick, xs, ys, BufferMaskBrick, Screen);
    } else {
        ScaledReBlitFromScreen(numbrick, xs, ys);
    }
}

/*--------------------------------------------------------------------------*/

void AffBrickBlock(S32 block, S32 brick, S16 x, S16 y, S16 z) {
    U8 *adr;
    S32 nb, col;
    register T_COLONB *ptrlbc;
    S16 numbrick;
    U8 brickcol;

    adr = GetAdrBlock(block);
    adr += HEADER_BLOCK;

    adr += (brick << 2); /* 4 Bytes to Jump	*/
    brickcol = *adr;
    adr += 2;

    numbrick = *(U16 *)adr; /* & 32767 */

    if (numbrick AND numbrick != ForbidenBrick) {
        Map2Screen(x - StartXCube, y - StartYCube, z - StartZCube);

        // Left clip is -47 (one full brick width minus a pixel), not -24 (half-brick).
        // Many iso bricks have hotX=0 or +24 — their visible pixel range at XScreen=-32
        // is [0,15], 16 pixels on screen. The historical -24 threshold assumed
        // hotX=-24 and was silently correct only at widths where
        // (ModeDesiredX/2 - 32) % 24 == 0 (4:3 / 640: 288 = 12*24). At wider widths
        // the iso origin offset slides off that multiple and the rejected bricks
        // start having visible content. See WIDESCREEN_HARDCODED_DIMS_AUDIT.md.
        // Clip bounds are native brick pixel dimensions (47 = full brick width-1,
        // 38 = brick height), scaled with the iso view via ISO_PX so wider tiles
        // at zoom are not wrongly rejected at the screen edges.
        if (XScreen >= -ISO_PX(47) AND XScreen < (S32)ModeDesiredX AND YScreen >= -ISO_PX(38) AND
                                                                                      YScreen < (S32)ModeDesiredY) {
            BlitBrickIso(numbrick - 1, XScreen, YScreen);

            // ListBrickColon is indexed by `col = (XScreen + isoCol)/isoCol`,
            // which goes negative below -isoCol and would corrupt the array.
            // Bricks in the newly-painted [-ISO_PX(47), -isoCol) strip aren't
            // bucketed for DrawOverBrick re-blits; acceptable, they sit on the
            // leftmost pixel column and were already painted above.
            S32 isoCol = ISO_PX(24); /* scaled half-brick column width (48/2 intercalée) */
            if (XScreen < -isoCol) {
                return;
            }

            col = (XScreen + isoCol) / isoCol;

            nb = NbBrickColon[col];

            ptrlbc = &ListBrickColon[col][nb];

            if (nb < MAX_BRICK) {
                ptrlbc->Xm = x;
                ptrlbc->Ym = y;
                ptrlbc->Zm = z;
                ptrlbc->Xs = (S16)XScreen;
                ptrlbc->Ys = (S16)YScreen;
                ptrlbc->Brick = (S16)(numbrick - 1);
                ptrlbc->Col = brickcol;
                NbBrickColon[col]++;
            } else
                Message("Arg MAX_BRICK Z BUFFER atteint", TRUE);
        }
    }
}

/*--------------------------------------------------------------------------*/
/*ptc = BufCube + y*2 + (x*SIZE_CUBE_Y*2) + (z*SIZE_CUBE_X*SIZE_CUBE_Y*2) ;*/

void AffGrille() {
    S32 block;
    U8 *ptc;
    S16 z, y, x;

    PtrProjectPoint(0, 0, 0);
    XpOrgw = Xp;
    YpOrgw = Yp;

    memset(NbBrickColon, 0, sizeof(S16) * NB_COLON);

    ptc = BufCube;
    for (z = 0; z < SIZE_CUBE_Z; z++) {
        for (x = 0; x < SIZE_CUBE_X; x++) {
            for (y = 0; y < SIZE_CUBE_Y; y++) {
                block = *ptc++;

                if (block) {
                    AffBrickBlock(block - 1, *ptc, x, y, z);
                }
                ptc++;
            }
        }
    }
}

/*══════════════════════════════════════════════════════════════════════════*
	  █     █▀▀▀█ █▀▀█  █  ▄▀ █▀▀▀█  █    ██▄ █ ▀▀█▀▀ █   █ █▀▀▀▀
	  ██    ██▀▀█ ██▀▀█ ██▀   ██▀█▀  ██   ██▀██   ██  ██▀▀█ ██▀▀
	  ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀    ▀▀  ▀  ▀▀   ▀▀  ▀   ▀▀  ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/*--------------------------------------------------------------------------*/
// Calcule les masques du Z-Buffer pour DrawOverBrick sans dessiner la
// brick
void AffBrickBlockColon(S32 block, S32 brick, S16 x, S16 y, S16 z) {
    U8 *adr;
    S32 nb, col;
    register T_COLONB *ptrlbc;
    S16 numbrick;
    U8 brickcol;

    adr = GetAdrBlock(block);
    adr += HEADER_BLOCK;

    adr += (brick << 2); /* 4 Bytes to Jump	*/
    brickcol = *adr;
    adr += 2;

    numbrick = *(U16 *)adr; /* & 32767 */

    if (numbrick AND numbrick != ForbidenBrick) {
        Map2Screen(x - StartXCube, y - StartYCube, z - StartZCube);

        if (XScreen >= -ISO_PX(24) AND XScreen < (S32)ModeDesiredX AND YScreen >= -ISO_PX(38) AND
                                                                                      YScreen < (S32)ModeDesiredY) {
            S32 isoCol = ISO_PX(24); /* scaled half-brick column width */
            col = (XScreen + isoCol) / isoCol;

            nb = NbBrickColon[col];

            ptrlbc = &ListBrickColon[col][nb];

            if (nb < MAX_BRICK) {
                ptrlbc->Xm = x;
                ptrlbc->Ym = y;
                ptrlbc->Zm = z;
                ptrlbc->Xs = (S16)XScreen;
                ptrlbc->Ys = (S16)YScreen;
                ptrlbc->Brick = (S16)(numbrick - 1);
                ptrlbc->Col = brickcol;
                NbBrickColon[col]++;

            } else
                Message("Arg MAX_BRICK Z BUFFER atteint", TRUE);
        }
    }
}

// Calcule le Z-Buffer pour le DrawOverBrick de toute la grille sans
// afficher les bricks
void AffGrilleOnly() {
    S32 block;
    U8 *ptc;
    U8 *adr;
    S16 z, y, x;
    U8 codejeu;

    PtrProjectPoint(0, 0, 0);
    XpOrgw = Xp;
    YpOrgw = Yp;

    memset(NbBrickColon, 0, sizeof(S16) * NB_COLON);

    ptc = BufCube;

    for (z = 0; z < SIZE_CUBE_Z; z++) {
        for (x = 0; x < SIZE_CUBE_X; x++) {
            for (y = 0; y < SIZE_CUBE_Y; y++) {
                block = *ptc++;

                if (block) {
                    block--;

                    //-------------------
                    adr = GetAdrBlock(block);
                    adr += (HEADER_BLOCK + 1);

                    codejeu = (U8)((*adr) >> 4);

                    if (codejeu != CJ_LABYRINTHE)
                    //-------------------
                    {
                        AffBrickBlockColon(block, *ptc, x, y, z);
                    }
                }
                ptc++;
            }
        }
    }
}
/*--------------------------------------------------------------------------*/
// Affiche la brick sans calculer le Z-Buffer pour DrawOverBrick
void AffBrickBlockOnly(S32 block, S32 brick, S16 x, S16 y, S16 z) {
    U8 *adr;
    S32 numbrick;

    adr = GetAdrBlock(block);
    adr += HEADER_BLOCK;

    adr += (brick << 2); /* 4 Bytes to Jump	*/
    adr += 2;

    numbrick = *(U16 *)adr; /* & 32767 */

    if (numbrick AND numbrick != ForbidenBrick) {
        Map2Screen(x - StartXCube, y - StartYCube, z - StartZCube);

        if (XScreen >= -ISO_PX(24) AND XScreen < (S32)ModeDesiredX AND YScreen >= -ISO_PX(38) AND
                                                                                      YScreen < (S32)ModeDesiredY) {
            BlitBrickIso(numbrick - 1, XScreen, YScreen);
        }
    }
}

// Redessine uniquement les block n'ayant pas le code jeu Invisible
void AffGrilleLabyrinthe() {
    S32 block;
    U8 *ptc;
    U8 *adr;
    S16 z, y, x;
    U8 codejeu;

    PtrProjectPoint(0, 0, 0);
    XpOrgw = Xp;
    YpOrgw = Yp;

    ptc = BufCube;

    for (z = 0; z < SIZE_CUBE_Z; z++) {
        for (x = 0; x < SIZE_CUBE_X; x++) {
            for (y = 0; y < SIZE_CUBE_Y; y++) {
                block = *ptc++;

                if (block) {
                    block--;

                    adr = GetAdrBlock(block);
                    adr += (HEADER_BLOCK + 1);

                    codejeu = (U8)((*adr) >> 4);

                    if (codejeu != CJ_LABYRINTHE) {
                        AffBrickBlockOnly(block, *ptc, x, y, z);
                    }
                }
                ptc++;
            }
        }
    }
}

// Affiche un cube de block (3*3*3) autour du point passe (coordonnees world)
void AffOneBrick(S32 x, S32 y, S32 z) {
    S32 block;
    U8 *ptc;
    U8 *adr;
    S32 xmin, ymin, xmax, ymax;
    S16 xs, ys, zs;
    S16 zdeb;
    U8 codejeu;

    // Sentinel seeds for the min-accumulator below. Must be at least as large as
    // any ScreenXMin/YMin a drawn brick could produce, i.e. the framebuffer right
    // and bottom edges. At 640x480 these are the original 639/479; route through
    // ModeDesiredX/Y so wider builds catch bricks past x=640. See Phase 2 in
    // docs/WIDESCREEN.md.
    xmin = (S32)ModeDesiredX - 1;
    ymin = (S32)ModeDesiredY - 1;
    xmax = 0;
    ymax = 0;

    PtrProjectPoint(0, 0, 0);
    XpOrgw = Xp;
    YpOrgw = Yp;

    x /= SIZE_BRICK_XZ;
    y /= SIZE_BRICK_Y;
    z /= SIZE_BRICK_XZ;

    zdeb = (S16)(z - 1);
    if (zdeb < 0)
        zdeb = 0;

    for (zs = zdeb; zs <= z + 1; zs++) {
        for (xs = (S16)(x - 1); xs <= (S16)(x + 1); xs++) {
            ptc = GetAdrColonneCube(xs, zs);
            ptc += (y - 1) * 2;
            for (ys = (S16)(y - 1); ys <= (S16)(y + 1); ys++) {
                block = *ptc++;
                if (block) {
                    block--;

                    adr = GetAdrBlock(block);
                    adr += (HEADER_BLOCK + 1);

                    codejeu = (U8)((*adr) >> 4);

                    if (codejeu == CJ_LABYRINTHE) {
                        AffBrickBlockOnly(block, *ptc, xs, ys, zs);
                        if (ScreenXMin < xmin)
                            xmin = ScreenXMin;
                        if (ScreenXMax > xmax)
                            xmax = ScreenXMax;
                        if (ScreenYMin < ymin)
                            ymin = ScreenYMin;
                        if (ScreenYMax > ymax)
                            ymax = ScreenYMax;
                    }
                }
                ptc++;
            }
        }
    }

    SetClip(xmin, ymin, xmax, ymax);
    DrawRecover((x + 1) * SIZE_BRICK_XZ, y * SIZE_BRICK_Y, (z + 1) * SIZE_BRICK_XZ, 0, 0);
}

// Fin Labyrinthe

/*--------------------------------------------------------------------------*/

void IncrustGrm(T_ZONE *ptrz) {
    S32 numgrm, xo, yo, zo;
    U8 *pts, *ptd;
    S32 x, z;
    S32 dx, dy, dz;
    char tmpFilePath[ADELINE_MAX_PATH];

    numgrm = ptrz->Info0;

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BKG_HQR_NAME);
    Load_HQR(tmpFilePath, Screen,
             BkgHeader.Grm_Start + GriHeader->My_Grm + numgrm);

    xo = ptrz->X0 / SIZE_BRICK_XZ;
    yo = ptrz->Y0 / SIZE_BRICK_Y;
    zo = ptrz->Z0 / SIZE_BRICK_XZ;

    pts = (U8 *)Screen;

    dx = *pts++;
    dy = *pts++;
    dz = *pts++;

    yo <<= 1; //	Cause S16
    dy <<= 1; //	Cause S16

    for (z = zo; z < zo + dz; z++) {
        for (x = xo; x < xo + dx; x++) {
            ptd = GetAdrColonneCube(x, z);
            ptd += yo;
            memcpy(ptd, pts, dy);
            pts += dy;
        }
    }

    FirstTime = AFF_ALL_FLIP;
}

/*--------------------------------------------------------------------------*/
void DesIncrustGrm(T_ZONE *ptrz) {
    U8 *pts, *ptd;
    U8 col[50]; //	1 Colonne decompressee
    S32 x, y, z, dy;
    S32 z0, z1, x0, x1;
    S32 y0, y1;

    x0 = ptrz->X0 / SIZE_BRICK_XZ;
    x1 = ptrz->X1 / SIZE_BRICK_XZ;

    y0 = ptrz->Y0 / SIZE_BRICK_Y;
    y1 = ptrz->Y1 / SIZE_BRICK_Y;

    z0 = ptrz->Z0 / SIZE_BRICK_XZ;
    z1 = ptrz->Z1 / SIZE_BRICK_XZ;

    y = y0 << 1; //	Cause S16

    dy = ((y1 - y0) + 1) << 1;

    /*-------------- Copy -------------------------*/
    for (z = z0; z <= z1; z++) {
        for (x = x0; x <= x1; x++) {
            pts = GetAdrColonneMap(x, z);
            DecompColonne(pts, col);

            ptd = GetAdrColonneCube(x, z);
            memcpy(ptd + y, col + y, dy);
        }
    }

    FirstTime = AFF_ALL_FLIP;
}

/*--------------------------------------------------------------------------*/
void RedrawGRMs(void) {
    S32 n;
    T_ZONE *ptrz = ListZone;

    for (n = 0; n < NbZones; n++, ptrz++) {
        switch (ptrz->Type) {
        case 3: // GRM
            if (ptrz->Info2) {
                IncrustGrm(ptrz);
            }
            break;
        }
    }
}
/*--------------------------------------------------------------------------*/

#include <stdio.h>

static S32 logical_shift_right_s32(S32 value, U32 bits) {
    return (S32)(((U32)value) >> bits);
}

static S32 arithmetic_shift_right_8(S32 value) {
    if (value >= 0) {
        return value >> 8;
    }

    return (S32)(-(((-(long long)value) + 255LL) >> 8));
}

U8 WorldColBrick(S32 xw, S32 yw, S32 zw) {
    XMap = logical_shift_right_s32(xw, 9);
    YMap = logical_shift_right_s32(yw, 8);
    ZMap = logical_shift_right_s32(zw, 9);

    if (XMap < 0 || XMap >= 64) {
        return 0;
    }

    int cubeIndex = (XMap * 50);

    if (YMap <= -1) {
        YMap = 0;
        return 1;
    } else if (YMap < 0 || YMap >= 25) {
        return 0;
    }

    cubeIndex += YMap * 2;

    if (ZMap < 0 || ZMap >= 64) {
        return 0;
    }

    cubeIndex += ZMap * 3200;

    U8 value = BufCube[cubeIndex];

    if (value == 0) {
        return BufCube[cubeIndex + 1];
    }

    U32 numBlock = value - 1;
    U8 *addr = GetAdrBlock(numBlock);
    addr += HEADER_BLOCK;
    addr += BufCube[cubeIndex + 1] * 4;

    return *addr;
}

//-------------------------------------------------------------------
U8 WorldColBrickFull(S32 xw, S32 yw, S32 zw, S32 ymax) {
    XMap = logical_shift_right_s32(xw, 9);
    YMap = arithmetic_shift_right_8(yw);
    ZMap = logical_shift_right_s32(zw, 9);

    if (XMap < 0 || XMap >= SIZE_CUBE_X)
        return 0;

    if (YMap <= -1) {
        return 1;
    }

    if (YMap >= SIZE_CUBE_Y)
        return 0;

    if (ZMap < 0 || ZMap >= SIZE_CUBE_Z)
        return 0;

    U32 colIdx = XMap * 50 + YMap * 2 + ZMap * 3200;

    U8 col = BufCube[colIdx];
    if (col == 0) {
        // eventuelle col transparente
        col = BufCube[colIdx + 1];

        // check full height
        U32 something = ((U32)(ymax + 255)) >> 8;
        S32 scanY = YMap;
        do {
            if (scanY >= SIZE_CUBE_Y - 1) {
                return col;
            }

            colIdx += 2;
            ++scanY;

            if (BufCube[colIdx] != 0 || BufCube[colIdx + 1] != 0) {
                return 1;
            }
            something--;
        } while (something != 0);

        return col;
    }

    S32 block = BufCube[colIdx] - 1;
    U8 *adr = GetAdrBlock(block);
    adr += HEADER_BLOCK;
    adr += BufCube[colIdx + 1] * 4; // 4 Bytes to Jump
    col = *adr;

    // check full height
    U32 something = ((U32)(ymax + 255)) >> 8;
    S32 scanY = YMap;
    do {
        if (scanY >= SIZE_CUBE_Y - 1) {
            return col;
        }

        colIdx += 2;
        ++scanY;

        if (BufCube[colIdx] != 0 || BufCube[colIdx + 1] != 0) {
            return 1;
        }
        something--;
    } while (something != 0);

    return col;
}

//-------------------------------------------------------------------
U8 WorldCodeBrick(S32 xw, S32 yw, S32 zw) {
    XMap = logical_shift_right_s32(xw, 9);

    U8 code = 0;

    if (yw > -1) {
        U8 *ptc = BufCube;

        YMap = logical_shift_right_s32(yw, 8);
        ZMap = logical_shift_right_s32(zw, 9);

        ptc += XMap * 50 + YMap * 2 + ZMap * 3200;

        U8 col = *ptc;
        if (col) {
            U8 *adr = GetAdrBlock(col - 1);
            adr += HEADER_BLOCK;
            code = *(adr + 1);
        }
    }
    return code;
}

//-------------------------------------------------------------------
U8 GetBlockBrick(S32 xw, S32 yw, S32 zw) {
    XMap = logical_shift_right_s32(xw, 9);
    YMap = logical_shift_right_s32(yw, 8);
    ZMap = logical_shift_right_s32(zw, 9);

    if (XMap < 0 || XMap >= 64) {
        return 0; // ovfw
    }

    if (YMap <= -1) {
        YMap = 0;
        return 1; // col_1
    }

    if (YMap < 0 || YMap >= 25) {
        return 0; // ovfw
    }

    if (ZMap < 0 || ZMap >= 64) {
        return 0; // ovfw
    }

    U32 offset = XMap * 50 + YMap * 2 + ZMap * 3200;
    U8 *ptc = BufCube + offset;

    return *ptc;
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
U8 GetWorldColBrickVisible(S32 x, S32 y, S32 z) {
    XMap = logical_shift_right_s32(x, 9);
    YMap = logical_shift_right_s32(y, 8);
    ZMap = logical_shift_right_s32(z, 9);

    if (XMap >= 0 && XMap < 64) {
        if (YMap <= -1) {
            YMap = 0;
            return 1;
        }
        if (YMap >= 0 && YMap < 25 && ZMap >= 0 && ZMap < 64) {
            U8 *ptc = 3200 * ZMap + 50 * XMap + BufCube + 2 * YMap;

            if (*ptc != 0) {
                U8 block = (*ptc++) - 1;
                U8 *adr = GetAdrBlock(block);
                adr += HEADER_BLOCK;
                adr += (*ptc) << 2; /* 4 Bytes to Jump	*/
                return *adr;
            }
        }
    }

    return 0;
}

//-------------------------------------------------------------------
void DecompColonne(U8 *pts, U8 *ptd) {
    U8 *dstData = ptd;
    U8 count = 0;
    U8 *srcData = pts;

    // Get Nb Entity
    U8 nbEntity = *srcData;
    srcData++;

    // For Every Entity
    do {
        // NewEntity:
        while (1) {
            U8 opCode = 0;
            while (1) {
                opCode = *srcData++;
                count = (opCode & 0x3F) + 1;
                // Incrust?
                if (opCode & 0xC0) {
                    break;
                }
                // Incrust ( Full off Nothing )
                opCode = 0;
                while (count) {
                    dstData[0] = 0;
                    dstData[1] = 0;
                    dstData += 2;
                    --count;
                }
                --nbEntity;
                if (!nbEntity) {
                    return;
                }
            }
            // Diff brick ?
            if (opCode & 0x40) {
                break;
            }
            U8 numBlock = srcData[0];
            U8 posBlock = srcData[1];
            srcData += 2;
            while (count) {
                dstData[0] = numBlock;
                dstData[1] = posBlock;
                dstData += 2;
                --count;
            }
            if (!--nbEntity) {
                return;
            }
        }
        while (count) {
            dstData[0] = srcData[0];
            dstData[1] = srcData[1];
            srcData += 2;
            dstData += 2;
            --count;
        }
        --nbEntity;
    } while (nbEntity);
}

//-------------------------------------------------------------------
// Map a brick's (x, y, z) world position onto the screen. The 24/12/15
// factors are the isometric step geometry: each grid step shifts the
// brick 24 px horizontally / 12 px vertically (the 2:1 iso tile shape),
// and each vertical world unit shifts -15 px screen-down. The constant
// offset places brick (0, 0, 0) on screen — historically at (288, 226)
// at the default 640×480, which is the framebuffer centre minus a
// (32, 14) shim from the iso grid's geometry (not a derivable formula;
// the shim is where the retail content expects the iso grid origin to
// land relative to the projection centre at SetIsoProjection's
// 320-9 = 311). Derive both axes from the framebuffer dimensions so the
// iso grid stays centred at any render-space size; at 640×480 this is
// the original (288, 226). See docs/WIDESCREEN.md Phase 2.
void Map2Screen(S32 x, S32 y, S32 z) {
    // Brick screen position, scaled by the runtime iso view scale so the floor
    // grid tracks the models (LongProjectPointIso uses the same IsoScale). At the
    // default ISO_SCALE_ONE the multiply/shift cancels and this is bit-for-bit the
    // original `24*(x-z)` / `12*(z+x)-15*y`. Only the iso offset scales; the screen
    // anchor (ModeDesiredX/2 ...) does not, so zoom stays centred on the view.
    // Note: the brick *tile* is still blitted at native size by AffBrickBlock, so
    // scales > 1 leave gaps between tiles. See docs/ISO_SCALE.md.
    XScreen = (S32)(((long long)(24 * (x - z)) * IsoScale) >> ISO_SCALE_SHIFT) +
              ((S32)ModeDesiredX / 2 - 32);
    YScreen = (S32)(((long long)(12 * (z + x) - 15 * y) * IsoScale) >> ISO_SCALE_SHIFT) +
              ((S32)ModeDesiredY / 2 - 14);
    Projrec_Map2Screen(x, y, z, XScreen, YScreen);
}

//-------------------------------------------------------------------
U8 *GetAdrBlock(S32 numblock) {
    return TabBlock + ((U32 *)TabBlock)[numblock];
}
