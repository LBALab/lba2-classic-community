#ifndef TERRAIN_H
#define TERRAIN_H

#define Y_SEA -50

extern S32 Sky_Y;
extern S32 Reglage;

/*--------------------------------------------------------------------------*/
extern void DrawSkySeaZBuf(void);
/*--------------------------------------------------------------------------*/
extern void ClsTerrainZBuf(S32 clearLog);
/*--------------------------------------------------------------------------*/
extern void DrawSeenSea(S32 y);
/*--------------------------------------------------------------------------*/
extern void DrawHorizon2ZBuf(void);
/*--------------------------------------------------------------------------*/
/* 1 when the last DrawHorizon2ZBuf had no missing-cube sea placeholder:
 * the backdrop is in its steady state and may be reused by the stereo
 * capture. */
extern S32 HorizonBackdropStable(void);
/*--------------------------------------------------------------------------*/
extern void AffichageTerrainZBuf(void);
/*--------------------------------------------------------------------------*/
extern void AffichageTerrainRapide(void);
/*--------------------------------------------------------------------------*/
extern void CopyBlock256(U8 *sour, U8 *dest);
/*--------------------------------------------------------------------------*/
extern void ClearAniPoly(void);
/*--------------------------------------------------------------------------*/
extern void DoAnimatedPolys();
/*--------------------------------------------------------------------------*/
extern void DrawAnimatedPolys();
/*--------------------------------------------------------------------------*/
extern void FiltreAnimatedPolys();
/*--------------------------------------------------------------------------*/

#endif // TERRAIN_H
