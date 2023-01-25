#include <SVGA/SDL.H>

#include <SVGA/DIRTYBOX.H> // For BoxStaticAdd and BoxCleanClip
#include <SVGA/SCREEN.H>   // For ModeDesiredX, ModeDesiredY and Log buffer
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/WINDOW.H> // For AppActive

#include <SDL2/SDL.h>
#include <assert.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define PAL_MAX_COLORS 256

// --- Public state ------------------------------------------------------------
void *Phys;
U32 ModeResX;
U32 ModeResY;
U32 TabOffPhysLine[ADELINE_MAX_Y_RES];

// --- Private state -----------------------------------------------------------
SDL_Surface *sdlSurface = NULL;
SDL_Palette *sdlPalette = NULL;
S32 screenLockCount = 0;

// --- Initialization ----------------------------------------------------------
bool InitVideo() {
  assert(sdlSurface == NULL);

  if (!IsWindowInitialized()) {
    LogPrintf("Error: Unable to initialize Video subsystem.\n"
              "\tWindow subsystem was not initialized beforehand.\n");

    return false;
  }

  return true;
}

void EndVideo() {
  if (sdlPalette) {
    SDL_FreePalette(sdlPalette);
    sdlPalette = NULL;
  }

  sdlSurface = NULL;
}

// --- Interface ---------------------------------------------------------------
bool CreateVideoSurface(U32 resX, U32 resY) {
  assert(sdlSurface == NULL);
  assert(sdlPalette == NULL);

  sdlSurface = (SDL_Surface *)(WindowSurface());
  if (sdlSurface == NULL) {
    LogPrintf("Error: Unable to get SDL video surface.\n"
              "\tResolution: %ux%u.\n",
              resX, resY);

    return false;
  }

  // Create palette
  sdlPalette = SDL_AllocPalette(PAL_MAX_COLORS);
  if (sdlPalette == NULL) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Error: Unable to create SDL palette.\n"
              "\tSDL message: %s\n",
              errorMsg);

    sdlSurface = NULL;

    return false;
  }

  // Update public state
  ModeResX = resX;
  ModeResY = resY;
  Phys = malloc(resX * resY);

  // Fill the phys table
  S32 off = 0;
  for (S32 i = 0; i < ADELINE_MAX_Y_RES; i++) {
    TabOffPhysLine[i] = off;
    off += VideoSurfacePitch();
  }

  return true;
}

U32 VideoSurfacePitch() {
  // TODO: Improve - SDL creates a surface with 32bpp, but code expects it to
  //  be 8bpp, for now just supply what is expected but this may change when
  //  port draw code to SDL blit
  return ModeResX;
}

void LockVideoSurface() {
  assert(sdlSurface != NULL);

  if (screenLockCount == 0) {
    SDL_LockSurface(sdlSurface);
    ++screenLockCount;
  }
}

void UnlockVideoSurface() {
  assert(sdlSurface != NULL);

  if (screenLockCount > 0) {
    --screenLockCount;
  }

  if (screenLockCount == 0) {
    SDL_UnlockSurface(sdlSurface);
  }
}

void WaitVideoSync() {
  // TODO: Implement?
}

/**
 * @param src color sequence R G B, each color in palette requires 3 src entries
 * @param startIdx initial index in the color palette
 * @param count number of palette colors to update
 */
void SetVideoPalette(const U8 src[], S32 startIdx, S32 count) {
  assert(sdlPalette != NULL);

  static SDL_Color tmpColors[PAL_MAX_COLORS];

  // Sanitize input
  if (startIdx < 0) {
    startIdx = 0;
  }

  // Update temporary color table
  for (U32 i = startIdx; i < count; ++i) {
    tmpColors[i].r = src[0];
    tmpColors[i].g = src[1];
    tmpColors[i].b = src[2];
    tmpColors[i].a = 0xFF;
    src += 3;
  }

  // Update palette
  int ret = SDL_SetPaletteColors(sdlPalette, tmpColors, startIdx, count);
  if (ret != 0) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Warning: Unable to set SDL palette color.\n"
              "\tSDL message: %s\n",
              errorMsg);
  }
}

void SetVideoPaletteCol(S32 colorIdx, U8 r, U8 g, U8 b) {
  assert(sdlPalette != NULL);

  // Check input
  if ((colorIdx < 0) || (colorIdx >= PAL_MAX_COLORS)) {
    return;
  }

  SDL_Color tmpColor = {r, g, b, 0xFF};

  int ret = SDL_SetPaletteColors(sdlPalette, &tmpColor, colorIdx, 1);
  if (ret != 0) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Warning: Unable to set SDL palette color.\n"
              "\tSDL message: %s\n",
              errorMsg);
  }
}

void SetVideoPaletteSync(const U8 src[]) {
  SetVideoPalette(src, 0, PAL_MAX_COLORS);

  T_BOX copyArea = {0, 0, 640, 480};
  CopyVideoArea(Phys, Log, TabOffPhysLine, &copyArea);
}

void CopyVideoArea(void *dst, const void *src, const U32 tabOffDst[],
                   const T_BOX *area) {
  assert(sdlSurface != NULL);
  assert(sdlPalette != NULL);
  assert((src != Phys) && "SDL Surface as 'src' is not supported");
  assert((src != dst) && "Copy to same video memory is not supported");

  U8 *dstMem = (dst == Phys) ? (U8 *)sdlSurface->pixels : (U8 *)dst;
  U8 *srcMem = (src == Phys) ? (U8 *)sdlSurface->pixels : (U8 *)src;
  U32 dstPitch = (dst == Phys) ? (U32)sdlSurface->pitch : tabOffDst[1];
  U32 srcPitch = (src == Phys) ? (U32)sdlSurface->pitch : tabOffDst[1];
  U8 dstBpp = (dst == Phys) ? sdlSurface->format->BytesPerPixel : 1;
  U8 srcBpp = (src == Phys) ? sdlSurface->format->BytesPerPixel : 1;

  if (dst == Phys) {
    LockVideoSurface();
  }

  for (int x = area->x0; x < area->x1; ++x) {
    for (int y = area->y0; y < area->y1; ++y) {

      // TODO Unify when using palette for SDL
      // TODO Make use of SDL Blit, must convert all buffers to SDL
      if (dst == Phys) { // Yes, I know... :D
        U8 *srcPixel = (srcMem + (y * srcPitch) + (x * srcBpp));
        SDL_Color srcPaletteColor = sdlPalette->colors[*srcPixel];

        U32 *dstPixel = (U32 *)(dstMem + (y * dstPitch) + (x * dstBpp));
        *dstPixel = SDL_MapRGBA(sdlSurface->format, srcPaletteColor.r,
                                srcPaletteColor.g, srcPaletteColor.b,
                                srcPaletteColor.a);
      } else {
        U8 *srcPixel = (srcMem + (y * srcPitch) + (x * srcBpp));
        U8 *dstPixel = (dstMem + (y * dstPitch) + (x * dstBpp));

        *dstPixel = *srcPixel;
      }
    }

    if (dst == Phys) {
      UnlockVideoSurface();
      UpdateWindowSurfaceArea(area, 1);
    }
  }
}

void HandleEventsVideo(const void *event) {
  assert(event != NULL);

  const SDL_Event *sdlEvent = (SDL_Event *)(event);

  if (sdlEvent->type == SDL_WINDOWEVENT) {
    switch (sdlEvent->window.event) {
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      if (!AppActive) {
        BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);
        BoxCleanClip = TRUE;
      }
      break;
    }
  }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
