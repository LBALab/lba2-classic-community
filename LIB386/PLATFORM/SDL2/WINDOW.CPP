#include "PRIVATE.H"

#include <limits.h>

namespace {
bool s_initialized = false;
const char *s_title = NULL;
SDL_Window *s_window = NULL;
SDL_Renderer *s_renderer = NULL;
SDL_Texture *s_texture = NULL;
uint32_t s_width = 0, s_height = 0;
bool s_fullscreen = false;
#ifdef LBA2_SDL2_TESTS
void (*s_presentProbe)(SDL_Renderer *) = NULL;
int (*s_fullscreenProbe)(SDL_Window *, Uint32) = NULL;
#endif

bool Error(const char *operation) {
    Platform_Log(PLATFORM_LOG_ERROR, "%s: %s", operation, SDL_GetError());
    return false;
}
bool Dimensions(uint32_t width, uint32_t height) {
    if (!width || !height || width > INT_MAX / sizeof(uint32_t) || height > INT_MAX) {
        SDL_SetError("Invalid presentation dimensions");
        return Error("SDL2 surface");
    }
    return true;
}
SDL_Rect Viewport(int width, int height) {
    SDL_Rect area = {0, 0, width, height};
    if ((double)width * s_height > (double)height * s_width) {
        area.w = (int)((double)height * s_width / s_height);
        area.x = (width - area.w) / 2;
    } else {
        area.h = (int)((double)width * s_height / s_width);
        area.y = (height - area.h) / 2;
    }
    return area;
}
int32_t Coordinate(int32_t value, int origin, int extent, uint32_t dimension) {
    const double mapped = ((double)value - origin) * dimension / extent;
    if (mapped <= 0)
        return 0;
    if (mapped >= dimension)
        return (int32_t)dimension - 1;
    return (int32_t)mapped;
}
bool PresentTexture() {
    int width, height;
    if (SDL_GetRendererOutputSize(s_renderer, &width, &height) != 0)
        return Error("Unable to query SDL2 render dimensions");
    const SDL_Rect area = Viewport(width, height);
    if (SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255) != 0 ||
        SDL_RenderClear(s_renderer) != 0 || SDL_RenderCopy(s_renderer, s_texture, NULL, &area) != 0)
        return Error("Unable to present SDL2 frame");
#ifdef LBA2_SDL2_TESTS
    if (s_presentProbe)
        s_presentProbe(s_renderer);
#endif
    SDL_RenderPresent(s_renderer);
    return true;
}
bool DisplaySize(int display, uint32_t *width, uint32_t *height) {
    SDL_DisplayMode mode;
    if (!width || !height || display < 0 || SDL_GetCurrentDisplayMode(display, &mode) != 0 ||
        mode.w <= 0 || mode.h <= 0)
        return false;
    *width = (uint32_t)mode.w;
    *height = (uint32_t)mode.h;
    return true;
}
} // namespace

#ifdef LBA2_SDL2_TESTS
SDL_Window *SDL2_TestWindow() { return s_window; }
void SDL2_SetPresentationProbe(void (*probe)(SDL_Renderer *)) { s_presentProbe = probe; }
void SDL2_SetFullscreenProbe(int (*probe)(SDL_Window *, Uint32)) { s_fullscreenProbe = probe; }
#endif

bool SDL2_InitWindow(const char *title) {
    if (!title) {
        SDL_SetError("Window title is null");
        return Error("Unable to initialize SDL2 window");
    }
    if (!SDL2_InitVideo())
        return Error("Unable to initialize SDL2 video");
    s_title = title;
    s_initialized = true;
    return true;
}
void SDL2_EndWindow() {
    if (s_initialized) {
        SDL2_DestroyWindow();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        s_initialized = false;
    }
}
bool SDL2_WindowInitialized() { return s_initialized; }
bool SDL2_CreateWindow(uint32_t width, uint32_t height, bool fullscreen) {
    if (!s_initialized || s_window || !Dimensions(width, height)) {
        SDL_SetError("Window is not ready for surface creation");
        return Error("SDL2 create");
    }
    s_window = SDL_CreateWindow(s_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                (int)width, (int)height, SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!s_window)
        return Error("Unable to create SDL2 window");
    s_fullscreen = fullscreen;
    s_renderer = SDL_CreateRenderer(s_window, -1, 0);
    if (!s_renderer) {
        SDL2_DestroyWindow();
        return Error("Unable to create SDL2 renderer");
    }
    SDL2_SetVSync(true);
    if (!SDL2_ResizeWindow(width, height, true)) {
        SDL2_DestroyWindow();
        return false;
    }
    SDL_ShowCursor(SDL_DISABLE);
    return true;
}
bool SDL2_ResizeWindow(uint32_t width, uint32_t height, bool keepSize) {
    if (!s_window || !s_renderer || !Dimensions(width, height))
        return false;
    SDL_Texture *texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, (int)width, (int)height);
    if (!texture)
        return Error("Unable to allocate SDL2 presentation texture");
    if (SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) != 0) {
        SDL_DestroyTexture(texture);
        return Error("Unable to configure SDL2 logical presentation");
    }
    SDL_DestroyTexture(s_texture);
    s_texture = texture;
    s_width = width;
    s_height = height;
    if (!keepSize)
        SDL_SetWindowSize(s_window, (int)width, (int)height);
    return true;
}
void SDL2_DestroyWindow() {
    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    s_texture = NULL;
    s_renderer = NULL;
    s_window = NULL;
    s_width = s_height = 0;
}
bool SDL2_WindowSize(int32_t *width, int32_t *height) {
    if (!s_window || !width || !height)
        return false;
    int w, h;
    SDL_GetWindowSize(s_window, &w, &h);
    *width = w;
    *height = h;
    return true;
}
bool SDL2_DisplaySize(uint32_t *width, uint32_t *height) {
    return s_window && DisplaySize(SDL_GetWindowDisplayIndex(s_window), width, height);
}
bool SDL2_PrimaryDisplaySize(uint32_t *width, uint32_t *height) {
    const char *driver = SDL_GetCurrentVideoDriver();
    return driver && SDL_strcmp(driver, "dummy") && SDL_strcmp(driver, "offscreen") &&
           DisplaySize(0, width, height);
}
void SDL2_SurfaceCoords(int32_t x, int32_t y, int32_t *sx, int32_t *sy) {
    if (!s_renderer || !s_width || !s_height) {
        *sx = x;
        *sy = y;
        return;
    }
    /* SDL_RenderSetLogicalSize rewrites native mouse and finger events.
       Explicit letterboxing preserves the contract's window-pixel coordinates. */
    int width, height;
    SDL_GetWindowSize(s_window, &width, &height);
    const SDL_Rect area = Viewport(width, height);
    if (area.w <= 0 || area.h <= 0) {
        *sx = *sy = 0;
        return;
    }
    *sx = Coordinate(x, area.x, area.w, s_width);
    *sy = Coordinate(y, area.y, area.h, s_height);
}
void SDL2_SetFullscreen(bool enabled) {
    const bool previous = s_fullscreen;
    s_fullscreen = enabled;
    if (!s_window)
        return;
    const Uint32 flags = enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    int result;
#ifdef LBA2_SDL2_TESTS
    if (s_fullscreenProbe)
        result = s_fullscreenProbe(s_window, flags);
    else
#endif
        result = SDL_SetWindowFullscreen(s_window, flags);
    if (result != 0) {
        s_fullscreen = previous;
        Platform_Log(PLATFORM_LOG_WARN, "Unable to change SDL2 fullscreen state: %s", SDL_GetError());
    } else if (s_renderer && s_texture)
        PresentTexture();
}
bool SDL2_GetFullscreen() { return s_fullscreen; }
bool SDL2_IsFullscreen() {
    return s_window && (SDL_GetWindowFlags(s_window) & SDL_WINDOW_FULLSCREEN) != 0;
}
void SDL2_SetVSync(bool enabled) {
    if (s_renderer && SDL_RenderSetVSync(s_renderer, enabled ? 1 : 0) != 0)
        Platform_Log(PLATFORM_LOG_WARN, "Unable to change SDL2 vsync: %s", SDL_GetError());
}
void SDL2_SetRelativeMouse(bool enabled) {
    if (s_window && SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) != 0)
        Platform_Log(PLATFORM_LOG_WARN, "Unable to change SDL2 relative mouse mode: %s", SDL_GetError());
}
void SDL2_HandleWindowEvent(const SDL_Event *event) {
    if (!s_window || event->type != SDL_WINDOWEVENT)
        return;
    if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED || event->window.event == SDL_WINDOWEVENT_ENTER)
        SDL_ShowCursor(SDL_DISABLE);
    else if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED && s_renderer && s_texture)
        PresentTexture();
}
bool SDL2_Present(const PlatformFrame *frame) {
    if (!frame || !s_texture || !s_renderer || frame->width != s_width || frame->height != s_height) {
        SDL_SetError("Indexed frame does not match the presentation surface");
        return Error("SDL2 present");
    }
    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(s_texture, NULL, &pixels, &pitch) != 0)
        return Error("Unable to lock SDL2 presentation texture");
    bool converted = Platform_ConvertIndexedFrame(frame, (uint32_t *)pixels, (uint32_t)pitch);
    if (converted && frame->overlay)
        frame->overlay((uint32_t *)pixels, (uint32_t)pitch, frame->width, frame->height, frame->palette_rgb);
    SDL_UnlockTexture(s_texture);
    return converted && PresentTexture();
}
