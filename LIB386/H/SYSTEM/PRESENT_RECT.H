#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

/* A rectangle in window (output) pixel space. The field layout mirrors
 * SDL_FRect so WINDOW.CPP can copy straight across, but this header carries no
 * SDL dependency: the windowed-present geometry is pure math and host-testable
 * without a real window or renderer (see tests/present_rect). */
typedef struct PresentRect {
    float x;
    float y;
    float w;
    float h;
} PresentRect;

/* Forward map. Centered, aspect-preserved destination rect for presenting a
 * surfaceW x surfaceH framebuffer into an outputW x outputH window (pillarbox
 * left/right when the window is wider than the framebuffer, letterbox
 * top/bottom when it is taller). Coordinates are snapped to whole pixels so a
 * static framebuffer presents identically frame-to-frame. See WINDOW.CPP for
 * the Wayland resize-jitter rationale. Falls back to the full surface rect when
 * the surface or output size is degenerate. */
void ComputePresentRect(int outputW, int outputH, U32 surfaceW, U32 surfaceH,
                        PresentRect *out);

/* Inverse of ComputePresentRect: maps a point in window (output) pixel space to
 * surface (framebuffer) pixel space, clamped to [0, surface-1]. Points inside
 * the pillarbox/letterbox bars clamp to the nearest framebuffer edge. Returns
 * the point unchanged when the surface size is degenerate. */
void WindowPointToSurface(S32 windowX, S32 windowY, int outputW, int outputH,
                          U32 surfaceW, U32 surfaceH, S32 *surfaceX,
                          S32 *surfaceY);

// =============================================================================
#ifdef __cplusplus
}
#endif
