#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
extern bool AppActive; ///< Application running and in focus status

// --- Initialization ----------------------------------------------------------
bool InitWindow(const char *title);
void EndWindow();
bool IsWindowInitialized();

// --- Interface ---------------------------------------------------------------
bool CreateWindowSurface(U32 resX, U32 resY);
void DestroyWindowSurface();
void PresentRendererFrame(const void *pixels, U32 pitch);
bool LockRendererTexture(void **pixels, int *pitch);
void UnlockAndPresentRendererTexture();
void WindowToSurfaceCoords(S32 windowX, S32 windowY, S32 *surfaceX, S32 *surfaceY);
void SetWindowFullscreen(bool fullscreen);
bool GetWindowFullscreen();

// Toggle renderer vsync at runtime. On (the default) paces presents to the display
// refresh; off lets the loop run flat-out. The control harness turns it off so a
// scripted run is not capped at ~60 fps -- presentation only, no effect on rendered
// pixels or simulation state.
void Window_SetVSync(int enabled);

void ManageWindow();
void HandleEventsWindow(const void *event);

// =============================================================================
#ifdef __cplusplus
}
#endif
