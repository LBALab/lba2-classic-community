#include <SYSTEM/WINDOW.H>

#include <PLATFORM/PLATFORM.H>
#include <assert.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
extern "C" {

// --- Public state ------------------------------------------------------------
bool AppActive = false;

// --- Initialization ----------------------------------------------------------
bool InitWindow(const char *title) { return g_platform.window.init(title); }
void EndWindow() { g_platform.window.shutdown(); }
bool IsWindowInitialized() { return g_platform.window.initialized(); }

// --- Interface ---------------------------------------------------------------
bool CreateWindowSurface(U32 resX, U32 resY, bool fullscreen) {
    return g_platform.window.create(resX, resY, fullscreen);
}
void DestroyWindowSurface() { g_platform.window.destroy(); }
bool ResizeWindowSurface(U32 width, U32 height, bool keepWindowSize) {
    return g_platform.window.resize(width, height, keepWindowSize);
}
bool Window_GetDisplayDimensions(U32 *width, U32 *height) {
    return g_platform.window.display_size(width, height);
}
bool Window_GetPrimaryDisplayDimensions(U32 *width, U32 *height) {
    return g_platform.window.primary_display_size(width, height);
}
void WindowToSurfaceCoords(S32 x, S32 y, S32 *sx, S32 *sy) {
    g_platform.window.surface_coords(x, y, sx, sy);
}
void SetWindowRelativeMouse(bool enabled) { g_platform.window.set_relative_mouse(enabled); }
void SetWindowFullscreen(bool fullscreen) { g_platform.window.set_fullscreen(fullscreen); }
bool GetWindowFullscreen() { return g_platform.window.get_fullscreen(); }
bool Window_IsFullscreen(void) { return g_platform.window.is_fullscreen(); }
bool Window_SupportsWindowedMode(void) { return g_platform.capabilities.windowed; }
bool Window_CanLoseNativeWindow(void) { return g_platform.capabilities.transient_window_loss; }
void Window_SetVSync(int enabled) { g_platform.window.set_vsync(enabled != 0); }
bool Window_SetPresentFilter(int mode) { return g_platform.window.set_present_filter((PlatformPresentFilter)mode); }
void ManageWindow() {}

void HandleEventsWindow(const PlatformEvent *event) {
    assert(event != NULL);
    if (!IsWindowInitialized())
        return;
    switch (event->type) {
    case PLATFORM_EVENT_WINDOW_FOCUS_GAINED:
        AppActive = true;
        AppFocusAudio_OnChange(true);
        break;
    case PLATFORM_EVENT_WINDOW_FOCUS_LOST:
        AppActive = false;
        AppFocusAudio_OnChange(false);
        break;
    case PLATFORM_EVENT_QUIT:
        exit(0); // TODO: Implement graceful exit
        break;
    default:
        break;
    }
}

// =============================================================================
}
