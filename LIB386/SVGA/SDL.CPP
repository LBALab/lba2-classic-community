#include <SVGA/VIDEO.H>

#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/WINDOW.H>

#include <PLATFORM/PLATFORM.H>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
extern "C" {

#define PAL_MAX_COLORS 256

// --- Public state ------------------------------------------------------------
void *Phys;
U32 ModeResX;
U32 ModeResY;
U32 TabOffPhysLine[ADELINE_MAX_Y_RES];

// --- Private state -----------------------------------------------------------
static bool paletteInitialized = false;
static U8 paletteRgb[PAL_MAX_COLORS * 3];
S32 screenLockCount = 0;
bool frameDirty = false;
static PrePresentFn s_prePresent = NULL;
static PrePresentFn s_overlayPrePresent = NULL;
static PresentTimingFn s_presentBegin = NULL;
static PresentTimingFn s_presentEnd = NULL;

static void PresentFrame();

// --- Initialization ----------------------------------------------------------
bool InitVideo() {
    if (!IsWindowInitialized()) {
        Log_Error("Unable to initialize Video subsystem.\n"
                  "\tWindow subsystem was not initialized beforehand.");
        return false;
    }
    return true;
}

void EndVideo() { DestroyVideoSurface(); }

void DestroyVideoSurface() {
    if (Phys) {
        free(Phys);
        Phys = NULL;
    }
    paletteInitialized = false;
}

bool ResizeVideoSurface(U32 newW, U32 newH) {
    if (Phys)
        free(Phys);
    Phys = calloc((size_t)newW, (size_t)newH);
    if (Phys == NULL) {
        Log_Error("Unable to allocate video surface at %ux%u", newW, newH);
        return false;
    }
    ModeResX = newW;
    ModeResY = newH;
    S32 off = 0;
    for (S32 i = 0; i < ADELINE_MAX_Y_RES; i++) {
        TabOffPhysLine[i] = off;
        off += (S32)newW;
    }
    return true;
}

// --- Interface ---------------------------------------------------------------
bool CreateVideoSurface(U32 resX, U32 resY) {
    assert(!paletteInitialized);
    Phys = calloc((size_t)resX, (size_t)resY);
    if (Phys == NULL) {
        Log_Error("Unable to allocate video surface at %ux%u", resX, resY);
        return false;
    }
    memset(paletteRgb, 255, sizeof(paletteRgb));
    paletteInitialized = true;
    ModeResX = resX;
    ModeResY = resY;
    S32 off = 0;
    for (S32 i = 0; i < ADELINE_MAX_Y_RES; i++) {
        TabOffPhysLine[i] = off;
        off += VideoSurfacePitch();
    }
    return true;
}

U32 VideoSurfacePitch() { return ModeResX; }
void LockVideoSurface() { ++screenLockCount; }

void UnlockVideoSurface() {
    if (screenLockCount > 0)
        --screenLockCount;
    if (screenLockCount == 0 && frameDirty) {
        if (s_presentBegin)
            s_presentBegin();
        PresentFrame();
        if (s_presentEnd)
            s_presentEnd();
        frameDirty = false;
    }
}

void WaitVideoSync() {}

void SetVideoPalette(const U8 src[], S32 startIdx, S32 count) {
    assert(paletteInitialized);
    static U8 tmpColors[PAL_MAX_COLORS * 3];
    if (startIdx < 0)
        startIdx = 0;
    if (src == NULL || count < 0 || count > PAL_MAX_COLORS || startIdx > PAL_MAX_COLORS) {
        Log_Warn("Invalid palette update: start=%d count=%d", startIdx, count);
        return;
    }
    /* Keep the palette API's source ordering: count bounds the temporary
       table's fill, and the upload starts at its first entry. */
    for (S32 i = startIdx; i < count; ++i) {
        tmpColors[i * 3] = src[0];
        tmpColors[i * 3 + 1] = src[1];
        tmpColors[i * 3 + 2] = src[2];
        src += 3;
    }
    if (count > PAL_MAX_COLORS - startIdx) {
        Log_Warn("Invalid palette update: start=%d count=%d", startIdx, count);
        return;
    }
    memcpy(paletteRgb + startIdx * 3, tmpColors, (size_t)count * 3);
}

void SetVideoPaletteCol(S32 colorIdx, U8 r, U8 g, U8 b) {
    assert(paletteInitialized);
    if (colorIdx < 0 || colorIdx >= PAL_MAX_COLORS)
        return;
    paletteRgb[colorIdx * 3] = r;
    paletteRgb[colorIdx * 3 + 1] = g;
    paletteRgb[colorIdx * 3 + 2] = b;
}

void SetVideoPaletteSync(const U8 src[]) { SetVideoPalette(src, 0, PAL_MAX_COLORS); }

void CopyVideoArea(void *dst, const void *src, const U32 tabOffDst[], const T_BOX *area) {
    assert((src != Phys) && "Presentation surface as 'src' is not supported");
    assert((src != dst) && "Copy to same video memory is not supported");
    if (dst == Phys)
        frameDirty = true;
    U8 *dstMem = (U8 *)dst;
    const U8 *srcMem = (const U8 *)src;
    U32 srcPitch = dst == Phys ? ModeDesiredX : tabOffDst[1];
    for (int y = area->y0; y < area->y1; ++y) {
        U8 *dstRow = dstMem + tabOffDst[y] + area->x0;
        const U8 *srcRow = srcMem + (y * srcPitch) + area->x0;
        memcpy(dstRow, srcRow, area->x1 - area->x0);
    }
}

void SetPrePresentCallback(PrePresentFn fn) { s_prePresent = fn; }
void SetOverlayPrePresentCallback(PrePresentFn fn) { s_overlayPrePresent = fn; }
void SetPresentTimingCallbacks(PresentTimingFn begin, PresentTimingFn end) {
    s_presentBegin = begin;
    s_presentEnd = end;
}

static void ApplyOverlays(U32 *pixels, U32 pitch, U32, U32, const U8 *palette) {
    if (s_prePresent)
        s_prePresent(pixels, pitch, ModeResX, ModeResY, palette);
    if (s_overlayPrePresent)
        s_overlayPrePresent(pixels, pitch, ModeResX, ModeResY, palette);
}

static void PresentFrame() {
    assert(paletteInitialized);
    assert(Phys != NULL);
    /* An overlay can execute a command that changes the palette. Its colour
       table must still describe the pixels already converted for this frame. */
    static U8 framePalette[PAL_MAX_COLORS * 3];
    memcpy(framePalette, paletteRgb, sizeof(framePalette));
    /* Log can hold an offscreen destination while only a growing zoom
       rectangle is copied to Phys. Pixels outside that rectangle stay visible. */
    PlatformFrame frame = {
        (const U8 *)Phys, ModeResX, ModeResY, VideoSurfacePitch(), framePalette,
        (s_prePresent || s_overlayPrePresent) ? ApplyOverlays : NULL};
    g_platform.window.present(&frame);
}

void HandleEventsVideo(const PlatformEvent *event) {
    assert(event != NULL);
    if (event->type == PLATFORM_EVENT_WINDOW_FOCUS_GAINED && !AppActive) {
        BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);
        BoxCleanClip = TRUE;
    }
}

// =============================================================================
}
