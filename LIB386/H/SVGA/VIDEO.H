#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LIMITS.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
extern void *Phys;
extern U32 ModeResX; ///< X resolution for Phys
extern U32 ModeResY; ///< Y resolution for Phys
extern U32 TabOffPhysLine[ADELINE_MAX_Y_RES]; ///< Line offsets for Phys

// --- Initialization ----------------------------------------------------------
bool InitVideo();
void EndVideo();

// --- Interface ---------------------------------------------------------------
bool CreateVideoSurface(U32 resX, U32 resY);
U32 VideoSurfacePitch();
void LockVideoSurface();
void UnlockVideoSurface();
void WaitVideoSync();
void SetVideoPalette(const U8 src[], S32 startIdx, S32 count);
void SetVideoPaletteCol(S32 colorIdx, U8 r, U8 g, U8 b);
void SetVideoPaletteSync(const U8 src[]);
void CopyVideoArea(void *dst, const void *src, const U32 tabOffDst[],
                   const T_BOX *area);

void HandleEventsVideo(const void *event);

/** Optional overlay: called after Log→frameBuffer, before present. pitch = frameBuffer row stride in bytes; palette_rgb = 256*3 bytes (R,G,B per entry). */
typedef void (*PrePresentFn)(U32 *frameBuffer, U32 pitch, U32 width, U32 height, const U8 *palette_rgb);
void SetPrePresentCallback(PrePresentFn fn);

// -----------------------------------------------------------------------------
#define Palette(Pal) SetVideoPalette(Pal, 0, 256)
#define PalOne(col, r, g, b) SetVideoPaletteCol(col, r, g, b)

// =============================================================================
#ifdef __cplusplus
}
#endif
