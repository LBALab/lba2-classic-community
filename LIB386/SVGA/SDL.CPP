#include <SVGA/SDL.H>

#include <SVGA/DIRTYBOX.H> // For BoxStaticAdd and BoxCleanClip
#include <SVGA/SCREEN.H>   // For ModeDesiredX, ModeDesiredY and Log buffer
#include <SVGA/STAGING_BUFFER.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/WINDOW.H>

#ifdef USE_GPURENDERER
#include <GL/RENDER_GL.H>
extern S32 GPURendererActive;
extern S32 DisplayTexFilter;
#endif

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
static PrePresentActiveFn s_prePresentActive = NULL;
static PresentTimingFn s_presentBegin = NULL;
static PresentTimingFn s_presentEnd = NULL;
static U32 paletteLUT[PAL_MAX_COLORS];
static U32 s_paletteGeneration = 1;
#if defined(__ANDROID__)
/* Persistent staging buffer for the Android present path (see
 * AcquirePresentTarget): reused across frames, resized on demand, freed in
 * DestroyVideoSurface. */
static StagingBuffer s_androidStaging = {NULL, 0, 0};
#endif

// --- Private functions -------------------------------------------------------
static void PresentFrame();

const U32 *GL_GetPaletteLUT(void) {
    return paletteLUT;
}

U32 GL_GetPaletteGeneration(void) {
    return s_paletteGeneration;
}

static void RebuildPaletteLUT() {
    if (!sdlPalette) {
        return;
    }
    for (int i = 0; i < PAL_MAX_COLORS; ++i) {
        SDL_Color c = sdlPalette->colors[i];
        paletteLUT[i] = ((U32)0xFF << 24) | ((U32)c.r << 16) | ((U32)c.g << 8) | c.b;
    }
    s_paletteGeneration++;
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

    static S32 s_unlockLogCount = 0;
    if (s_unlockLogCount < 3) {
        Log_Info("UnlockVideoSurface: lockCount=%d frameDirty=%d", screenLockCount, (int)frameDirty);
        s_unlockLogCount++;
    }
    if (screenLockCount == 0 && frameDirty) {
        if (s_unlockLogCount < 3) {
            Log_Info("UnlockVideoSurface: calling PresentFrame");
        }
        if (s_presentBegin)
            s_presentBegin();
        PresentFrame();
        if (s_presentEnd)
            s_presentEnd();
        frameDirty = false;
    }
#ifdef USE_GPURENDERER
    else if (screenLockCount == 0 && GL_GetStereoEye() >= 0) {
        /* Stereo pair: eye 0's present consumed frameDirty, and eye 1's 2D
         * Log is identical to eye 0's, so without this the right eye's
         * composite would be skipped and its capture FBO would go stale
         * while the left eye stays fresh. Each eye of a pair must present. */
        PresentFrame();
    }
#endif
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
    s_paletteGeneration++;
}

void SetVideoPaletteSync(const U8 src[]) {
    if (sdlPalette == NULL) {
        return; /* video not initialized yet; nothing to sync */
    }
    SetVideoPalette(src, 0, PAL_MAX_COLORS);
    /* The GL renderer bakes scene colors from this palette into its
       framebuffer, so a palette change recolors the whole scene.  Frames
       that only redraw bodies over preserved terrain would otherwise keep
       showing terrain shaded with the previous palette — black after a
       load, until a camera move forces a full redraw. */
#ifdef USE_GPURENDERER
    if (GPURendererActive) {
        BoxStaticAdd(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1);
    }
#endif
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

void SetPrePresentActiveCallback(PrePresentActiveFn fn) {
    s_prePresentActive = fn;
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

/* ── Software frame limiter ─────────────────────────────────────────────── */

static S32 s_frameLimit = 0;
static U64 s_frameLimitNext = 0;

void Window_SetFrameLimit(int fps) {
    if (fps < 0)
        fps = 0;
    s_frameLimit = fps;
    s_frameLimitNext = 0;
}

/* Called at the end of every presented frame. With vsync on the swap has
   usually already blocked out most of the budget, so the wait below shrinks
   to nothing; with vsync off this is the only pacing. The spin tail keeps
   90/120 fps targets accurate: SDL_Delay alone rounds up to the OS
   scheduler tick. */
static void FrameLimitWait(void) {
    U64 now;

    if (s_frameLimit <= 0) {
        s_frameLimitNext = 0;
        return;
    }

    const U64 freq = SDL_GetPerformanceFrequency();
    const U64 frame = freq / (U64)s_frameLimit;
    now = SDL_GetPerformanceCounter();

    if (s_frameLimitNext == 0 || now + frame < s_frameLimitNext) {
        /* First frame, or a long stall (modal hang, load): restart the
           cadence from now instead of burst-catching-up. */
        s_frameLimitNext = now + frame;
        return;
    }

    if (now < s_frameLimitNext) {
        const U64 remainingNs =
            (s_frameLimitNext - now) * 1000000000ULL / freq;
        if (remainingNs > 3000000ULL) {
            SDL_Delay((Uint32)((remainingNs - 2000000ULL) / 1000000ULL));
        }
        while ((now = SDL_GetPerformanceCounter()) < s_frameLimitNext) {
        }
    }

    s_frameLimitNext += frame;
}

static void PresentFrame() {
    assert(sdlPalette != NULL);
    assert(Log != NULL);

#ifdef USE_GPURENDERER
    if (GPURendererActive) {
        /* Begin new GL frame: clear depth buffer before any 3D drawing. */
        GL_BeginFrame();

        /* GL path: convert 8-bit → ARGB on CPU, then upload via GL texture
           with optional bilinear filtering. The palette conversion is the
           same work as the SDL path; the difference is GL handles the
           upscale filtering. */

        /* Overlay callbacks (console overlay, etc.) draw ARGB directly, so
           they need the CPU-converted staging buffer. Without them the
           indexed path uploads the raw palette indices and expands them on
           the GPU — the CPU per-pixel loop disappears. The active predicate
           keeps even a registered-but-idle console (the normal state while
           playing) on the indexed path. */
        if ((s_prePresent && (!s_prePresentActive || s_prePresentActive()))
                || s_overlayPrePresent) {
            static U32 *s_glStaging = NULL;
            static U32 s_glStagingSize = 0;
            U32 needed = ModeDesiredX * ModeDesiredY;
            if (s_glStagingSize < needed) {
                free(s_glStaging);
                s_glStaging = (U32 *)malloc(needed * sizeof(U32));
                s_glStagingSize = s_glStaging ? needed : 0;
            }
            if (s_glStaging == NULL) {
                return;
            }

            /* Stereo eye 1 presents the same Log as eye 0 — reuse eye 0's
               converted staging instead of reconverting ~3M pixels per
               frame (the overlay texture is also skipped for eye 1 in
               GL_Present). */
            if (GL_GetStereoEye() == 1) {
                GL_Present(s_glStaging, ModeDesiredX * 4, ModeDesiredX, ModeDesiredY,
                           DisplayTexFilter);
                if (GL_GetStereoEye() != 0)
                    FrameLimitWait();
                return;
            }

            /* Palette index 0 is the transparent colour: the 3D FBO content
               shows through wherever the 2D overlay has index 0.  Force alpha
               to 0 so those pixels are fully transparent when composited. */
            paletteLUT[0] &= 0x00FFFFFF;

            U8 *logMem = (U8 *)Log;
            for (U32 y = 0; y < ModeDesiredY; ++y) {
                U8 *srcRow = logMem + y * ModeDesiredX;
                U32 *dstRow = s_glStaging + y * ModeDesiredX;
                for (U32 x = 0; x < ModeDesiredX; ++x) {
                    dstRow[x] = paletteLUT[srcRow[x]];
                }
            }

            static U8 paletteRgb[256 * 3];
            for (int i = 0; i < 256; i++) {
                U32 c = paletteLUT[i];
                paletteRgb[i * 3 + 0] = (U8)(c >> 16);
                paletteRgb[i * 3 + 1] = (U8)(c >> 8);
                paletteRgb[i * 3 + 2] = (U8)(c);
            }
            if (s_prePresent)
                s_prePresent(s_glStaging, ModeDesiredX * 4, ModeResX, ModeResY, paletteRgb);
            if (s_overlayPrePresent)
                s_overlayPrePresent(s_glStaging, ModeDesiredX * 4, ModeResX, ModeResY, paletteRgb);

            GL_Present(s_glStaging, ModeDesiredX * 4, ModeDesiredX, ModeDesiredY,
                       DisplayTexFilter);
            if (GL_GetStereoEye() != 0)
                FrameLimitWait();
            return;
        }

        /* Palette index 0 is the transparent colour: the 3D FBO content
           shows through wherever the 2D overlay has index 0.  Force alpha
           to 0 so the indexed-present shader composites it transparently. */
        paletteLUT[0] &= 0x00FFFFFF;

        GL_PresentIndexed((const U8 *)Log, ModeDesiredX, ModeDesiredX,
                           ModeDesiredY, paletteLUT, DisplayTexFilter);
        if (GL_GetStereoEye() != 0)
            FrameLimitWait();
        return;
    }
#endif

    void *stagingPixels;
    U32 stagingPitch;
    if (!AcquirePresentTarget(&stagingPixels, &stagingPitch)) {
        return;
    }

    U8 *logMem = (U8 *)Log;

    for (U32 y = 0; y < ModeDesiredY; ++y) {
        U8 *srcRow = logMem + y * ModeDesiredX;
        U32 *dstRow = (U32 *)((U8 *)stagingPixels + y * stagingPitch);
        for (U32 x = 0; x < ModeDesiredX; ++x) {
            dstRow[x] = paletteLUT[srcRow[x]];
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
    FrameLimitWait();
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

void PresentCurrentFrame(void) {
    PresentFrame();
}

// =============================================================================
#ifdef __cplusplus
}
#endif
