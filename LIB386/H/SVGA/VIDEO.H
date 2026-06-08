#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LIMITS.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
extern void *Phys;
extern U32 ModeResX;                          ///< X resolution for Phys
extern U32 ModeResY;                          ///< Y resolution for Phys
extern U32 TabOffPhysLine[ADELINE_MAX_Y_RES]; ///< Line offsets for Phys

// --- Initialization ----------------------------------------------------------
bool InitVideo();
void EndVideo();

// --- Interface ---------------------------------------------------------------
bool CreateVideoSurface(U32 resX, U32 resY);

/** Free the Phys buffer and the SDL palette without taking the SDL_INIT_VIDEO
 *  subsystem down — paired with CreateVideoSurface for the runtime resolution
 *  switcher (docs/RUNTIME_RESOLUTION.md). EndVideo() is still the lifetime
 *  teardown called from EndAdeline. */
void DestroyVideoSurface();

/** Resize the Phys buffer in place (free + re-malloc at newW * newH) and
 *  recompute TabOffPhysLine. Palette stays. Used by the runtime resolution
 *  switcher; pairs with WINDOW.H ResizeWindowSurface. Returns true on
 *  success. */
bool ResizeVideoSurface(U32 newW, U32 newH);
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

/** Optional overlay: called after Log→ARGB conversion, before present. pitch = row stride in bytes; palette_rgb = 256*3 bytes (R,G,B per entry). */
typedef void (*PrePresentFn)(U32 *frameBuffer, U32 pitch, U32 width, U32 height, const U8 *palette_rgb);
void SetPrePresentCallback(PrePresentFn fn);

/** Optional SCENE-level pre-present pass, called BEFORE the pre-present overlay
 *  (so it sits under the console/touch overlays). Same signature. Used by the
 *  experimental iso xBR brick compositor to overwrite brick pixels in the
 *  finished frame with an RGB-filtered layer. */
void SetScenePresentCallback(PrePresentFn fn);

/** Optional overlay rendered after the main pre-present callback. Used by the
 *  touch virtual gamepad overlay on Android so it draws on top of the console
 *  overlay. Same signature as PrePresentFn. */
void SetOverlayPrePresentCallback(PrePresentFn fn);

/** Optional timing taps around the present path (palette LUT scan + texture upload + GL submit + vsync wait).
    Both callbacks must be set together (or both NULL); called at most once per UnlockVideoSurface. */
typedef void (*PresentTimingFn)(void);
void SetPresentTimingCallbacks(PresentTimingFn begin, PresentTimingFn end);

// -----------------------------------------------------------------------------
#define Palette(Pal) SetVideoPalette(Pal, 0, 256)
#define PalOne(col, r, g, b) SetVideoPaletteCol(col, r, g, b)

// =============================================================================
#ifdef __cplusplus
}
#endif
