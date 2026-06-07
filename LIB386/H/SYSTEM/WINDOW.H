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
/** Create the SDL window, renderer, and streaming texture at resX*resY. When
 *  `fullscreen` is true the window is created fullscreen up front (via the
 *  SDL create-time property), avoiding a windowed-then-flipped frame on boot. */
bool CreateWindowSurface(U32 resX, U32 resY, bool fullscreen);
void DestroyWindowSurface();

/** Resize the existing window + streaming texture in place, without
 *  destroying the SDL window itself. Used by the runtime resolution switcher
 *  (docs/RUNTIME_RESOLUTION.md / SOURCES/RES_SWITCH.CPP) so a switch keeps
 *  focus, multi-monitor placement, and (in fullscreen) the fullscreen state
 *  without a destroy/recreate round-trip. Returns true on success. */
bool ResizeWindowSurface(U32 newW, U32 newH);

/** Query the current display's logical dimensions (the desktop the SDL
 *  window is on, via SDL_GetCurrentDisplayMode). Writes both outputs and
 *  returns true on success. Returns false if SDL hasn't initialised a
 *  window yet; *outW and *outH are untouched in that case. */
bool Window_GetDisplayDimensions(U32 *outW, U32 *outH);
void PresentRendererFrame(const void *pixels, U32 pitch);
bool LockRendererTexture(void **pixels, int *pitch);
void UnlockAndPresentRendererTexture();
void WindowToSurfaceCoords(S32 windowX, S32 windowY, S32 *surfaceX, S32 *surfaceY);
void SetWindowFullscreen(bool fullscreen);
bool GetWindowFullscreen();
// Actual SDL window fullscreen state (queries SDL_GetWindowFlags), not the
// cached request flag — for accurate reporting.
bool Window_IsFullscreen(void);

// Whether this platform has a meaningful windowed/fullscreen choice. True on
// desktop; false on always-fullscreen devices (Android phones/TVs), where a
// "Windowed" option is meaningless. Lets UI hide the fullscreen toggle without
// platform #ifdefs at the call site.
bool Window_SupportsWindowedMode(void);

// Toggle renderer vsync at runtime. On (the default) paces presents to the display
// refresh; off lets the loop run flat-out. The control harness turns it off so a
// scripted run is not capped at ~60 fps -- presentation only, no effect on rendered
// pixels or simulation state.
void Window_SetVSync(int enabled);

void SetWindowRelativeMouse(bool enable);
void ManageWindow();
void HandleEventsWindow(const void *event);

// Pause audio while the window is not focused. The window layer feeds focus
// changes to AppFocusAudio_OnChange(); the audio/game layer registers what to
// pause (on focus lost) and resume (on focus gained). A once-per-cycle guard
// inside AppFocusAudio_OnChange ensures one pause and one resume per focus
// cycle, mirroring the original PauseMiles/ResumeMiles + `milespaused`. The
// policy lives in FOCUS_AUDIO.CPP (no SDL) so the guard is host-testable.
// Either hook may be NULL.
typedef void (*AppFocusAudioFn)(void);
void SetAppFocusAudioHooks(AppFocusAudioFn onFocusLost, AppFocusAudioFn onFocusGained);
void AppFocusAudio_OnChange(bool gainedFocus);

// =============================================================================
#ifdef __cplusplus
}
#endif
