#include <SVGA/SDL.H>

#include <SVGA/DIRTYBOX.H> // For BoxStaticAdd and BoxCleanClip
#include <SVGA/SCREEN.H>   // For ModeDesiredX, ModeDesiredY and Log buffer
#include <SVGA/STAGING_BUFFER.H>
#include <SYSTEM/LOG.H>
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
static PrePresentFn s_overlayPrePresent = NULL;
static PresentTimingFn s_presentBegin = NULL;
static PresentTimingFn s_presentEnd = NULL;
static U32 paletteLUT[PAL_MAX_COLORS];
#if defined(__ANDROID__)
/* Persistent staging buffer for the Android present path (see
 * AcquirePresentTarget): reused across frames, resized on demand, freed in
 * DestroyVideoSurface. */
static StagingBuffer s_androidStaging = {NULL, 0, 0};
#endif

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
        Log_Error("Unable to initialize Video subsystem.\n"
                  "\tWindow subsystem was not initialized beforehand.");

        return false;
    }

    return true;
}

void EndVideo() {
    DestroyVideoSurface();
}

void DestroyVideoSurface() {
#if defined(__ANDROID__)
    StagingBufferFree(&s_androidStaging);
#endif
    if (Phys) {
        free(Phys);
        Phys = NULL;
    }
    if (sdlPalette) {
        SDL_DestroyPalette(sdlPalette);
        sdlPalette = NULL;
    }
}

bool ResizeVideoSurface(U32 newW, U32 newH) {
    /* The palette survives a resize -- it's per-frame state, not
       dimension-dependent. Only Phys (the present buffer) needs to change
       size, plus TabOffPhysLine which is the per-row offset table the
       blitter walks. */
    if (Phys) {
        free(Phys);
    }
    Phys = malloc((size_t)newW * (size_t)newH);
    if (Phys == NULL) {
        return false;
    }
    ModeResX = newW;
    ModeResY = newH;
    S32 off = 0;
    for (S32 i = 0; i < ADELINE_MAX_Y_RES; i++) {
        TabOffPhysLine[i] = off;
        off += (S32)newW; /* VideoSurfacePitch() returns ModeResX. */
    }
    return true;
}

// --- Interface ---------------------------------------------------------------
bool CreateVideoSurface(U32 resX, U32 resY) {
    assert(sdlPalette == NULL);

    sdlPalette = SDL_CreatePalette(PAL_MAX_COLORS);
    if (sdlPalette == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to create SDL palette.\n"
                  "\tSDL message: %s",
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
        if (s_presentBegin)
            s_presentBegin();
        PresentFrame();
        if (s_presentEnd)
            s_presentEnd();
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
        Log_Warn("Unable to set SDL palette color.\n"
                 "\tSDL message: %s",
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
        Log_Warn("Unable to set SDL palette color.\n"
                 "\tSDL message: %s",
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

void SetOverlayPrePresentCallback(PrePresentFn fn) {
    s_overlayPrePresent = fn;
}

void SetPresentTimingCallbacks(PresentTimingFn begin, PresentTimingFn end) {
    s_presentBegin = begin;
    s_presentEnd = end;
}

/* --- Present target: the one real platform difference, confined --------------
 * Desktop locks the renderer texture and writes ARGB straight into it
 * (zero-copy), then unlocks and presents. Android can't: on Android 16+ with
 * MTE, SDL_LockTexture can hand back texture-internal memory that is freed and
 * reallocated between frames, so the pointer carries a stale MTE tag
 * (SEGV_ACCERR). Android instead stages into a persistent buffer (reused across
 * frames, reallocated only on a mode change -- no per-frame allocation) and
 * uploads with SDL_UpdateTexture. PresentFrame itself stays platform-agnostic:
 * acquire a write target + its pitch, fill it, commit. */
#if defined(__ANDROID__)
static bool AcquirePresentTarget(void **pixels, U32 *pitch) {
    void *p = StagingBufferEnsure(&s_androidStaging, ModeDesiredX, ModeDesiredY);
    if (p == NULL) {
        return false;
    }
    *pixels = p;
    *pitch = ModeDesiredX * sizeof(U32);
    return true;
}

static void CommitPresentTarget(void *pixels, U32 pitch) {
    PresentRendererFrame(pixels, pitch);
}
#else
static bool AcquirePresentTarget(void **pixels, U32 *pitch) {
    int texPitch;
    if (!LockRendererTexture(pixels, &texPitch)) {
        return false;
    }
    *pitch = (U32)texPitch;
    return true;
}

static void CommitPresentTarget(void * /*pixels*/, U32 /*pitch*/) {
    UnlockAndPresentRendererTexture();
}
#endif

S32 ScreenZoom = ZOOM_ONE;

void SetScreenZoom(S32 zoom) {
    if (zoom < ZOOM_ONE)
        zoom = ZOOM_ONE; /* zoom in only; no minify */
    if (zoom > 16 * ZOOM_ONE)
        zoom = 16 * ZOOM_ONE;
    ScreenZoom = zoom;
}

static void PresentFrame() {
    assert(sdlPalette != NULL);
    assert(Log != NULL);

    void *stagingPixels;
    U32 stagingPitch;
    if (!AcquirePresentTarget(&stagingPixels, &stagingPitch)) {
        return;
    }

    U8 *logMem = (U8 *)Log;

    if (ScreenZoom == ZOOM_ONE) {
        for (U32 y = 0; y < ModeDesiredY; ++y) {
            U8 *srcRow = logMem + y * ModeDesiredX;
            U32 *dstRow = (U32 *)((U8 *)stagingPixels + y * stagingPitch);
            for (U32 x = 0; x < ModeDesiredX; ++x) {
                dstRow[x] = paletteLUT[srcRow[x]];
            }
        }
    } else {
        /* Nearest-neighbour magnify of a centred sub-region of Log to fill the
           frame. srcW/srcH is the window sampled (smaller = more zoom); stepX/Y
           are 16.16 source-pixels per destination pixel. */
        U32 W = ModeDesiredX, H = ModeDesiredY;
        U32 srcW = (U32)((U64)W * ZOOM_ONE / (U32)ScreenZoom);
        U32 srcH = (U32)((U64)H * ZOOM_ONE / (U32)ScreenZoom);
        if (srcW < 1)
            srcW = 1;
        if (srcH < 1)
            srcH = 1;
        U32 srcX0 = (W - srcW) / 2;
        U32 srcY0 = (H - srcH) / 2;
        U32 stepX = (srcW << 16) / W;
        U32 stepY = (srcH << 16) / H;
        for (U32 y = 0; y < H; ++y) {
            U32 srcY = srcY0 + ((y * stepY) >> 16);
            U8 *srcRow = logMem + srcY * W;
            U32 *dstRow = (U32 *)((U8 *)stagingPixels + y * stagingPitch);
            U32 accX = srcX0 << 16;
            for (U32 x = 0; x < W; ++x) {
                dstRow[x] = paletteLUT[srcRow[accX >> 16]];
                accX += stepX;
            }
        }
    }

    if (s_prePresent || s_overlayPrePresent) {
        /* Build the 256-entry RGB table once; both callbacks take the same. */
        static U8 paletteRgb[256 * 3];
        for (int i = 0; i < 256; i++) {
            U32 c = paletteLUT[i];
            paletteRgb[i * 3 + 0] = (U8)(c >> 16);
            paletteRgb[i * 3 + 1] = (U8)(c >> 8);
            paletteRgb[i * 3 + 2] = (U8)(c);
        }
        if (s_prePresent)
            s_prePresent((U32 *)stagingPixels, stagingPitch, ModeResX, ModeResY, paletteRgb);
        if (s_overlayPrePresent)
            s_overlayPrePresent((U32 *)stagingPixels, stagingPitch, ModeResX, ModeResY, paletteRgb);
    }

    CommitPresentTarget(stagingPixels, stagingPitch);
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
