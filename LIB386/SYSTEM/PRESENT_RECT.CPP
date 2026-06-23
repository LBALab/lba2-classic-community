/* Windowed-present geometry, factored out of WINDOW.CPP so it can be exercised
 * without a real SDL window/renderer (tests/present_rect). WINDOW.CPP wraps
 * these in the SDL-facing ComputePresentationRect / WindowToSurfaceCoords,
 * supplying the live window size; the math itself lives here and stays
 * SDL-free.
 *
 * Keeping the forward (present) and inverse (input) transforms side by side in
 * one TU is the point: they must agree, and a golden test pins that agreement
 * so a later swap to SDL_SetRenderLogicalPresentation can be validated as a
 * behaviour-preserving refactor. */

#include <SYSTEM/PRESENT_RECT.H>

#include <assert.h>
#include <cstddef> // NULL

void ComputePresentRect(int outputW, int outputH, U32 surfaceW, U32 surfaceH,
                        PresentRect *out) {
    assert(out != NULL);

    out->x = 0.0f;
    out->y = 0.0f;
    out->w = (float)surfaceW;
    out->h = (float)surfaceH;

    if ((surfaceW == 0) || (surfaceH == 0)) {
        return;
    }
    if ((outputW <= 0) || (outputH <= 0)) {
        return;
    }

    const double surfaceAspect = (double)surfaceW / (double)surfaceH;
    const double outputAspect = (double)outputW / (double)outputH;

    /* Compute as ints then promote to float. SDL_FRect + NEAREST sample mode
       can shift the rasterised output by 1 px frame-to-frame if the dst
       coordinates carry sub-pixel components and the window size jitters
       slightly (Wayland compositors do this during resize settle, even
       when the framebuffer is steady). Rounding to whole pixels here pins
       the present rect so a still framebuffer presents identically every
       frame, fixing a visible flicker on the runtime-resolution-switch
       modal at non-native (non-1:1) display modes. The tiny aspect drift
       this introduces (< 1 px in one dimension) is invisible. */
    int dstW, dstH, dstX, dstY;
    if (outputAspect > surfaceAspect) {
        dstH = outputH;
        dstW = (int)(dstH * surfaceAspect + 0.5);
        dstX = (outputW - dstW) / 2;
        dstY = 0;
    } else {
        dstW = outputW;
        dstH = (int)(dstW / surfaceAspect + 0.5);
        dstX = 0;
        dstY = (outputH - dstH) / 2;
    }
    out->x = (float)dstX;
    out->y = (float)dstY;
    out->w = (float)dstW;
    out->h = (float)dstH;
}

void WindowPointToSurface(S32 windowX, S32 windowY, int outputW, int outputH,
                          U32 surfaceW, U32 surfaceH, S32 *surfaceX,
                          S32 *surfaceY) {
    assert(surfaceX != NULL);
    assert(surfaceY != NULL);

    if ((surfaceW == 0) || (surfaceH == 0)) {
        *surfaceX = windowX;
        *surfaceY = windowY;
        return;
    }

    PresentRect dstRect;
    ComputePresentRect(outputW, outputH, surfaceW, surfaceH, &dstRect);

    if ((dstRect.w <= 0.0f) || (dstRect.h <= 0.0f)) {
        *surfaceX = windowX;
        *surfaceY = windowY;
        return;
    }

    float relX = (float)windowX - dstRect.x;
    float relY = (float)windowY - dstRect.y;

    if (relX < 0.0f) {
        relX = 0.0f;
    } else if (relX > (dstRect.w - 1.0f)) {
        relX = dstRect.w - 1.0f;
    }

    if (relY < 0.0f) {
        relY = 0.0f;
    } else if (relY > (dstRect.h - 1.0f)) {
        relY = dstRect.h - 1.0f;
    }

    *surfaceX = (S32)(relX * ((float)surfaceW / dstRect.w));
    *surfaceY = (S32)(relY * ((float)surfaceH / dstRect.h));

    if (*surfaceX >= (S32)surfaceW) {
        *surfaceX = (S32)surfaceW - 1;
    }
    if (*surfaceY >= (S32)surfaceH) {
        *surfaceY = (S32)surfaceH - 1;
    }
}
