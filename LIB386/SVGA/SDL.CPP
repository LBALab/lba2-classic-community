#include <SVGA/SDL.H>

#include <SVGA/DIRTYBOX.H> // For BoxStaticAdd and BoxCleanClip
#include <SVGA/SCREEN.H>   // For ModeDesiredX, ModeDesiredY and Log buffer
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/WINDOW.H>

#include <SDL3/SDL.h>
#include <assert.h>
#include <cstdlib>
#include <string.h>

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
SDL_Palette *sdlPalette = NULL;
S32 screenLockCount = 0;
bool frameDirty = false;
U32 *frameBuffer = NULL;
static PrePresentFn s_prePresent = NULL;

// --- Private functions -------------------------------------------------------
static void PresentFrame();

// --- Initialization ----------------------------------------------------------
bool InitVideo() {
  if (!IsWindowInitialized()) {
    LogPrintf("Error: Unable to initialize Video subsystem.\n"
              "\tWindow subsystem was not initialized beforehand.\n");

    return false;
  }

  return true;
}

void EndVideo() {
  if (sdlPalette) {
    SDL_DestroyPalette(sdlPalette);
    sdlPalette = NULL;
  }

  if (frameBuffer) {
    free(frameBuffer);
    frameBuffer = NULL;
  }
}

// --- Interface ---------------------------------------------------------------
bool CreateVideoSurface(U32 resX, U32 resY) {
  assert(sdlPalette == NULL);

  sdlPalette = SDL_CreatePalette(PAL_MAX_COLORS);
  if (sdlPalette == NULL) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Error: Unable to create SDL palette.\n"
              "\tSDL message: %s\n",
              errorMsg);

    return false;
  }

  ModeResX = resX;
  ModeResY = resY;

  Phys = malloc(resX * resY);

  frameBuffer = (U32 *)malloc(resX * resY * sizeof(U32));
  if (!frameBuffer) {
    LogPrintf("Error: Unable to allocate frame buffer.\n");
    return false;
  }
  memset(frameBuffer, 0, resX * resY * sizeof(U32));

  S32 off = 0;
  for (S32 i = 0; i < ADELINE_MAX_Y_RES; i++) {
    TabOffPhysLine[i] = off;
    off += VideoSurfacePitch();
  }

  return true;
}

U32 VideoSurfacePitch() {
  return ModeResX;
}

void LockVideoSurface() {
  ++screenLockCount;
}

void UnlockVideoSurface() {
  if (screenLockCount > 0) {
    --screenLockCount;
  }

  if (screenLockCount == 0 && frameDirty) {
    PresentFrame();
    frameDirty = false;
  }
}

void WaitVideoSync() {
}

/**
 * @param src color sequence R G B, each color in palette requires 3 src entries
 * @param startIdx initial index in the color palette
 * @param count number of palette colors to update
 */
void SetVideoPalette(const U8 src[], S32 startIdx, S32 count) {
  assert(sdlPalette != NULL);

  static SDL_Color tmpColors[PAL_MAX_COLORS];

  if (startIdx < 0) {
    startIdx = 0;
  }

  for (U32 i = startIdx; i < count; ++i) {
    tmpColors[i].r = src[0];
    tmpColors[i].g = src[1];
    tmpColors[i].b = src[2];
    tmpColors[i].a = 0xFF;
    src += 3;
  }

  if (!SDL_SetPaletteColors(sdlPalette, tmpColors, startIdx, count)) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Warning: Unable to set SDL palette color.\n"
              "\tSDL message: %s\n",
              errorMsg);
  }
}

void SetVideoPaletteCol(S32 colorIdx, U8 r, U8 g, U8 b) {
  assert(sdlPalette != NULL);

  if ((colorIdx < 0) || (colorIdx >= PAL_MAX_COLORS)) {
    return;
  }

  SDL_Color tmpColor = {r, g, b, 0xFF};

  if (!SDL_SetPaletteColors(sdlPalette, &tmpColor, colorIdx, 1)) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Warning: Unable to set SDL palette color.\n"
              "\tSDL message: %s\n",
              errorMsg);
  }
}

void SetVideoPaletteSync(const U8 src[]) {
  SetVideoPalette(src, 0, PAL_MAX_COLORS);
  PresentFrame();
}

void CopyVideoArea(void *dst, const void *src, const U32 tabOffDst[],
                   const T_BOX *area) {
  assert((src != Phys) && "SDL Surface as 'src' is not supported");
  assert((src != dst) && "Copy to same video memory is not supported");

  if (dst == Phys) {
    frameDirty = true;
    return;
  }

  U8 *dstMem = (U8 *)dst;
  U8 *srcMem = (U8 *)src;
  U32 pitch = tabOffDst[1];

  for (int y = area->y0; y < area->y1; ++y) {
    U8 *srcRow = srcMem + (y * pitch) + area->x0;
    U8 *dstRow = dstMem + (y * pitch) + area->x0;
    memcpy(dstRow, srcRow, area->x1 - area->x0);
  }
}

void SetPrePresentCallback(PrePresentFn fn) {
  s_prePresent = fn;
}

static void PresentFrame() {
  assert(sdlPalette != NULL);
  assert(frameBuffer != NULL);
  assert(Log != NULL);

  U8 *logMem = (U8 *)Log;

  for (U32 y = 0; y < ModeDesiredY; ++y) {
    U8 *srcRow = logMem + y * ModeDesiredX;
    U32 *dstRow = frameBuffer + y * ModeResX;
    for (U32 x = 0; x < ModeDesiredX; ++x) {
      SDL_Color c = sdlPalette->colors[srcRow[x]];
      dstRow[x] = ((U32)0xFF << 24) | ((U32)c.r << 16) | ((U32)c.g << 8) | c.b;
    }
  }

  if (s_prePresent) {
    static U8 paletteRgb[256 * 3];
    for (int i = 0; i < 256; i++) {
      paletteRgb[i * 3 + 0] = (U8)sdlPalette->colors[i].r;
      paletteRgb[i * 3 + 1] = (U8)sdlPalette->colors[i].g;
      paletteRgb[i * 3 + 2] = (U8)sdlPalette->colors[i].b;
    }
    s_prePresent(frameBuffer, ModeResX * (U32)sizeof(U32), ModeResX, ModeResY, paletteRgb);
  }

  PresentRendererFrame(frameBuffer, ModeResX * sizeof(U32));
}

void HandleEventsVideo(const void *event) {
  assert(event != NULL);

  const SDL_Event *sdlEvent = (SDL_Event *)(event);
  switch (sdlEvent->type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      if (!AppActive) {
        BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);
        BoxCleanClip = TRUE;
      }
      break;
    default:
      break;
  }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
