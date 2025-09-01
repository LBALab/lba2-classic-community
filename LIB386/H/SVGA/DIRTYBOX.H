#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Init:
//
// BoxInit( ... );
//
// processing loop:
//
// for( ...
// {
//    BoxMovingAdd( ... ) ;
//    AND/OR
//    BoxStaticAdd( ... ) ;
// }
//
// BoxUpdate() ;
//

// -----------------------------------------------------------------------------
extern S32 BoxClipXMin, BoxClipXMax, BoxClipYMin, BoxClipYMax;
extern S32 BoxCleanClip;

extern BOX_FUNC *BoxOneBlit, *BoxOneClean;
extern VOID_FUNC *BoxScreenGet, *BoxScreenRelease, *BoxScreenFlip;

// -----------------------------------------------------------------------------
S32 BoxInit(S32 nbBox);
void BoxMovingAdd(S32 x0, S32 y0, S32 x1, S32 y1);
void BoxStaticAdd(S32 x0, S32 y0, S32 x1, S32 y1);
void BoxUnstaticAdd(S32 x0, S32 y0, S32 x1, S32 y1);
void BoxCls(S32 x0, S32 y0, S32 x1, S32 y1);
void BoxStaticFullflip();
void BoxBlitStaticListToScreen();
void BoxBlit();
void BoxClean();
void BoxUpdate();
void BoxReset();
void BoxChangeClip(S32 x0, S32 y0, S32 x1, S32 y1, S32 clean);

extern void DefaultBoxOneBlit(T_BOX *pbox);
extern void DefaultBoxOneClean(T_BOX *pbox);
extern void DefaultBoxOneClear(T_BOX *pbox);

// -----------------------------------------------------------------------------
#define Flip BoxStaticFullflip

#define Cls() BoxCls(BoxClipXMin, BoxClipYMin, BoxClipXMax - 1, BoxClipYMax - 1)

#define SetBoxScreenGet(boxscreenget)                                          \
  BoxScreenGet = ((boxscreenget) ? (boxscreenget) : LockPrimary)

#define SetBoxScreenRelease(boxscreenrelease)                                  \
  BoxScreenRelease = ((boxscreenrelease) ? (boxscreenrelease) : UnlockPrimary)

#define SetBoxScreenFlip(boxscreenflip) BoxScreenFlip = (boxscreenflip)

#define SetBoxOneBlit(boxoneblit)                                              \
  BoxOneBlit = ((boxoneblit) ? (boxoneblit) : DefaultBoxOneBlit)

#define SetBoxOneClean(boxoneclean)                                            \
  BoxOneClean = ((boxoneclean) ? (boxoneclean) : DefaultBoxOneClean)

// =============================================================================
#ifdef __cplusplus
}
#endif
