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
static PrePresentFn s_prePresent = NULL;
static U32 paletteLUT[PAL_MAX_COLORS];

// --- Private functions -------------------------------------------------------
static void PresentFrame();

static void RebuildPaletteLUT() {
    if (!sdlPalette) {
        return;
    }
    for (int i = 0; i < PAL_MAX_COLORS; ++i) {
        SDL_Color c = sdlPalette->colors[i];
        paletteLUT[i] = ((U32)0xFF << 24) | ((U32)c.r << 16) | ((U32)c.g << 8) | c.b;
    }
}

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

    RebuildPaletteLUT();
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

    paletteLUT[colorIdx] = ((U32)0xFF << 24) | ((U32)r << 16) | ((U32)g << 8) | b;
}

void SetVideoPaletteSync(const U8 src[]) {
    SetVideoPalette(src, 0, PAL_MAX_COLORS);
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
    assert(Log != NULL);

    void *texPixels;
    int texPitch;

    if (!LockRendererTexture(&texPixels, &texPitch)) {
        return;
    }

    U8 *logMem = (U8 *)Log;

    for (U32 y = 0; y < ModeDesiredY; ++y) {
        U8 *srcRow = logMem + y * ModeDesiredX;
        U32 *dstRow = (U32 *)((U8 *)texPixels + y * texPitch);
        for (U32 x = 0; x < ModeDesiredX; ++x) {
            dstRow[x] = paletteLUT[srcRow[x]];
        }
    }

    if (s_prePresent) {
        static U8 paletteRgb[256 * 3];
        for (int i = 0; i < 256; i++) {
            U32 c = paletteLUT[i];
            paletteRgb[i * 3 + 0] = (U8)(c >> 16);
            paletteRgb[i * 3 + 1] = (U8)(c >> 8);
            paletteRgb[i * 3 + 2] = (U8)(c);
        }
        s_prePresent((U32 *)texPixels, (U32)texPitch, ModeResX, ModeResY, paletteRgb);
    }

    UnlockAndPresentRendererTexture();
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
