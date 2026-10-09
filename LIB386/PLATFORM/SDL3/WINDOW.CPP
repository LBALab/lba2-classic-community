#include "PRIVATE.H"

#include <PLATFORM/STAGING_BUFFER.H>
#include <SYSTEM/ANDROID.H>

#include <assert.h>

namespace {
bool s_initialized = false;
const char *s_title = NULL;
SDL_Window *s_window = NULL;
SDL_Renderer *s_renderer = NULL;
SDL_Texture *s_texture = NULL;
uint32_t s_width = 0;
uint32_t s_height = 0;
bool s_fullscreen = false;
#ifdef LBA2_SDL3_TESTS
void (*s_presentationProbe)(SDL_Renderer *) = NULL;
#endif
#if defined(__ANDROID__) || defined(LBA2_SDL3_TEST_STAGING)
StagingBuffer s_staging = {NULL, 0, 0};
#endif

void HideGameCursor(void) {
    SDL_HideCursor();
    /* Some WMs show the default arrow until the cursor is cleared. */
    SDL_SetCursor(NULL);
}

void SetLogicalPresentation(uint32_t width, uint32_t height) {
    if (!SDL_SetRenderLogicalPresentation(s_renderer, (int)width, (int)height,
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        Platform_Log(PLATFORM_LOG_WARN, "Unable to set SDL logical presentation to %ux%u.\n"
                                        "\tSDL message: %s",
                     width, height, SDL_GetError());
    }
}

bool PresentTexture(void) {
    if (!SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255) ||
        !SDL_RenderClear(s_renderer) ||
        !SDL_RenderTexture(s_renderer, s_texture, NULL, NULL)) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to present SDL frame: %s", SDL_GetError());
        return false;
    }
#ifdef LBA2_SDL3_TESTS
    if (s_presentationProbe)
        s_presentationProbe(s_renderer);
#endif
    if (!SDL_RenderPresent(s_renderer)) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to present SDL frame: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool CoerceFullscreen(bool fullscreen) {
    return g_platform.capabilities.windowed ? fullscreen : true;
}
} // namespace

extern "C" {

#ifdef LBA2_SDL3_TESTS
void SDL3_SetPresentationProbe(void (*probe)(SDL_Renderer *)) {
    s_presentationProbe = probe;
}
#endif

bool SDL3_InitWindow(const char *title) {
    assert(title != NULL);
    assert(!s_initialized);
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to initialize SDL Window/Video subsystem.\n"
                                         "\tSDL Message: %s",
                     SDL_GetError());
        return false;
    }
    s_title = title;
    s_initialized = true;
    return true;
}

void SDL3_EndWindow(void) {
    if (s_initialized) {
        SDL3_DestroyWindow();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        s_initialized = false;
    }
}

bool SDL3_IsWindowInitialized(void) { return s_initialized; }

bool SDL3_CreateWindow(uint32_t width, uint32_t height, bool fullscreen) {
    assert(s_initialized);
    assert(s_window == NULL);
    fullscreen = CoerceFullscreen(fullscreen);
    s_fullscreen = fullscreen;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, s_title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
#ifdef __ANDROID__
    /* Resizable permits portrait rotation, which can race surface teardown.
       A TV needs fullscreen at creation to avoid a later EGL surface transition;
       on phones the same property can race the native window's mutex. */
    if (IsAndroidTVDevice())
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
#else
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, fullscreen);
#endif
    s_window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (s_window == NULL) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to create SDL window.\n"
                                         "\tResolution: %ux%u.\n\tSDL message: %s",
                     width, height, SDL_GetError());
        return false;
    }
    if (s_fullscreen && !SDL_SetWindowFullscreen(s_window, true)) {
        Platform_Log(PLATFORM_LOG_WARN, "Unable to switch SDL window to fullscreen.\n"
                                        "\tSDL message: %s",
                     SDL_GetError());
        s_fullscreen = false;
    }
    s_renderer = SDL_CreateRenderer(s_window, NULL);
    if (s_renderer == NULL) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to create SDL renderer.\n"
                                         "\tSDL message: %s",
                     SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        return false;
    }
    SDL_SetRenderVSync(s_renderer, 1);
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, width, height);
    if (s_texture == NULL) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to create SDL streaming texture.\n"
                                         "\tSize: %ux%u.\n\tSDL message: %s",
                     width, height, SDL_GetError());
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        return false;
    }
    SDL_SetTextureScaleMode(s_texture, SDL_SCALEMODE_NEAREST);
    s_width = width;
    s_height = height;
    SetLogicalPresentation(width, height);
    HideGameCursor();
    return true;
}

bool SDL3_DisplaySize(uint32_t *outW, uint32_t *outH) {
    if (outW == NULL || outH == NULL || s_window == NULL)
        return false;
    SDL_DisplayID display = SDL_GetDisplayForWindow(s_window);
    if (display == 0)
        return false;
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    if (mode == NULL || mode->w <= 0 || mode->h <= 0)
        return false;
    *outW = (uint32_t)mode->w;
    *outH = (uint32_t)mode->h;
    return true;
}

bool SDL3_PrimaryDisplaySize(uint32_t *outW, uint32_t *outH) {
    if (outW == NULL || outH == NULL || !SDL_WasInit(SDL_INIT_VIDEO))
        return false;
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver == NULL || SDL_strcmp(driver, "dummy") == 0 || SDL_strcmp(driver, "offscreen") == 0)
        return false;
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display == 0)
        return false;
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    if (mode == NULL || mode->w <= 0 || mode->h <= 0)
        return false;
    *outW = (uint32_t)mode->w;
    *outH = (uint32_t)mode->h;
    return true;
}

bool SDL3_WindowSize(int32_t *width, int32_t *height) {
    if (s_window == NULL || width == NULL || height == NULL)
        return false;
    return SDL_GetWindowSize(s_window, width, height);
}

bool SDL3_ResizeWindow(uint32_t width, uint32_t height, bool keepWindowSize) {
    if (s_window == NULL || s_renderer == NULL)
        return false;
    /* Window size and framebuffer size are independent. Automatic per-scene
       downshifts leave the player's window alone; their explicit choice moves it. */
    if (!keepWindowSize)
        SDL_SetWindowSize(s_window, (int)width, (int)height);
    if (s_texture) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, (int)width, (int)height);
    if (s_texture == NULL) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to recreate SDL streaming texture at %ux%u.\n"
                                         "\tSDL message: %s",
                     width, height, SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(s_texture, SDL_SCALEMODE_NEAREST);
    s_width = width;
    s_height = height;
    SetLogicalPresentation(width, height);
    return true;
}

void SDL3_DestroyWindow(void) {
#if defined(__ANDROID__) || defined(LBA2_SDL3_TEST_STAGING)
    StagingBufferFree(&s_staging);
#endif
    if (s_texture) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }
    if (s_renderer) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
    s_width = 0;
    s_height = 0;
}

void SDL3_SurfaceCoords(int32_t x, int32_t y, int32_t *sx, int32_t *sy) {
    assert(sx != NULL && sy != NULL);
    if (s_renderer == NULL || s_width == 0 || s_height == 0) {
        *sx = x;
        *sy = y;
        return;
    }
    float logicalX = 0.0f;
    float logicalY = 0.0f;
    SDL_RenderCoordinatesFromWindow(s_renderer, (float)x, (float)y, &logicalX, &logicalY);
    int32_t mappedX = (int32_t)logicalX;
    int32_t mappedY = (int32_t)logicalY;
    if (mappedX < 0)
        mappedX = 0;
    else if (mappedX >= (int32_t)s_width)
        mappedX = (int32_t)s_width - 1;
    if (mappedY < 0)
        mappedY = 0;
    else if (mappedY >= (int32_t)s_height)
        mappedY = (int32_t)s_height - 1;
    *sx = mappedX;
    *sy = mappedY;
}

void SDL3_SetRelativeMouse(bool enabled) {
    if (s_window)
        SDL_SetWindowRelativeMouseMode(s_window, enabled);
}

void SDL3_SetFullscreen(bool fullscreen) {
    fullscreen = CoerceFullscreen(fullscreen);
    s_fullscreen = fullscreen;
    if (s_window == NULL)
        return;
    if (!SDL_SetWindowFullscreen(s_window, fullscreen)) {
        Platform_Log(PLATFORM_LOG_WARN, "Unable to change SDL fullscreen state to %d.\n"
                                        "\tSDL message: %s",
                     fullscreen ? 1 : 0, SDL_GetError());
        s_fullscreen = !fullscreen;
        return;
    }
    if (s_renderer && s_texture)
        PresentTexture();
}

bool SDL3_GetFullscreen(void) { return s_fullscreen; }
bool SDL3_IsFullscreen(void) {
    return s_window && ((SDL_GetWindowFlags(s_window) & SDL_WINDOW_FULLSCREEN) != 0);
}

void SDL3_SetVSync(bool enabled) {
    if (s_renderer)
        SDL_SetRenderVSync(s_renderer, enabled ? 1 : 0);
}

void SDL3_HandleWindowEvent(const PlatformEvent *event) {
    if (!s_initialized)
        return;
    switch (event->type) {
    case PLATFORM_EVENT_WINDOW_FOCUS_GAINED:
    case PLATFORM_EVENT_WINDOW_MOUSE_ENTER:
        if (s_window)
            HideGameCursor();
        break;
    case PLATFORM_EVENT_WINDOW_RESIZED:
        if (s_renderer && s_texture)
            PresentTexture();
        break;
    default:
        break;
    }
}

bool SDL3_Present(const PlatformFrame *frame) {
    if (frame == NULL || frame->width != s_width || frame->height != s_height ||
        s_renderer == NULL || s_texture == NULL) {
        SDL_SetError("Indexed frame does not match the presentation surface");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    void *pixels = NULL;
    uint32_t pitch = 0;
#if defined(__ANDROID__) || defined(LBA2_SDL3_TEST_STAGING)
    /* MTE can invalidate SDL_LockTexture's storage between frames. Persistent
       owned memory plus an upload keeps the write pointer's tag stable. */
    pixels = StagingBufferEnsure(&s_staging, frame->width, frame->height);
    if (pixels == NULL) {
        SDL_SetError("Unable to allocate presentation staging buffer");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    pitch = frame->width * sizeof(uint32_t);
#else
    int texturePitch = 0;
    if (!SDL_LockTexture(s_texture, NULL, &pixels, &texturePitch)) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to lock presentation texture: %s", SDL_GetError());
        return false;
    }
    pitch = (uint32_t)texturePitch;
#endif
    bool converted = Platform_ConvertIndexedFrame(frame, (uint32_t *)pixels, pitch);
    if (converted && frame->overlay)
        frame->overlay((uint32_t *)pixels, pitch, frame->width, frame->height, frame->palette_rgb);
#if defined(__ANDROID__) || defined(LBA2_SDL3_TEST_STAGING)
    if (converted && !SDL_UpdateTexture(s_texture, NULL, pixels, (int)pitch)) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to upload presentation texture: %s", SDL_GetError());
        return false;
    }
#else
    SDL_UnlockTexture(s_texture);
#endif
    return converted && PresentTexture();
}
}
