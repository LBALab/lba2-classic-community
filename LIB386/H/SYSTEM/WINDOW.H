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

void SetWindowRelativeMouse(bool enable);
void ManageWindow();
void HandleEventsWindow(const void *event);

// =============================================================================
#ifdef __cplusplus
}
#endif
