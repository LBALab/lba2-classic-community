#include <SYSTEM/WINDOW.H>

#include <SYSTEM/ANDROID.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/PRESENT_RECT.H>

#include <SDL3/SDL.h>
#include <assert.h>
#include <cstdlib>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// TODO: Implement detection of app instance already running

// --- Public state ------------------------------------------------------------
bool AppActive = false;

// --- Private state -----------------------------------------------------------
bool windowSystemInitialized = false;
const char *windowTitle = NULL;
SDL_Window *sdlWindow = NULL;
SDL_Renderer *sdlRenderer = NULL;
SDL_Texture *sdlTexture = NULL;
U32 windowSurfaceResX = 0;
U32 windowSurfaceResY = 0;
bool windowFullscreenEnabled = false;

static void ApplyGameWindowHideSystemCursor(void) {
    SDL_HideCursor();
    /* Clear default cursor; some WMs still show an arrow until this runs. */
    SDL_SetCursor(NULL);
}

/* Window output size in pixels, or (0, 0) when there is no window yet. The
   present-rect math treats a zero output size as degenerate and falls back to
   the full surface rect, so the no-window case needs no special handling at the
   call sites below. */
static void GetWindowOutputSize(int *outputW, int *outputH) {
    *outputW = 0;
    *outputH = 0;
    if (sdlWindow != NULL) {
        SDL_GetWindowSize(sdlWindow, outputW, outputH);
    }
}

/* Thin SDL-facing wrapper over ComputePresentRect: supplies the live window
   size and copies the result into an SDL_FRect for the renderer. The geometry
   itself lives in PRESENT_RECT.CPP (SDL-free, host-tested). */
static void ComputePresentationRect(SDL_FRect *dstRect) {
    assert(dstRect != NULL);

    int outputW, outputH;
    GetWindowOutputSize(&outputW, &outputH);

    PresentRect rect;
    ComputePresentRect(outputW, outputH, windowSurfaceResX, windowSurfaceResY, &rect);

    dstRect->x = rect.x;
    dstRect->y = rect.y;
    dstRect->w = rect.w;
    dstRect->h = rect.h;
}

static void PresentCurrentTexture() {
    assert(sdlRenderer != NULL);
    assert(sdlTexture != NULL);

    SDL_FRect dstRect;

    ComputePresentationRect(&dstRect);

    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);
    SDL_RenderTexture(sdlRenderer, sdlTexture, NULL, &dstRect);
    SDL_RenderPresent(sdlRenderer);
}

// --- Initialization ----------------------------------------------------------
bool InitWindow(const char *title) {
    assert(title != NULL);
    assert(windowSystemInitialized == false);

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to initialize SDL Window/Video subsystem.\n"
                  "\tSDL Message: %s",
                  errorMsg);

        windowSystemInitialized = false;
        return false;
    }

    windowTitle = title;
    windowSystemInitialized = true;
    return true;
}

void EndWindow() {
    if (windowSystemInitialized) {
        DestroyWindowSurface();

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        windowSystemInitialized = false;
    }
}

bool IsWindowInitialized() {
    return windowSystemInitialized;
}

// --- Interface ---------------------------------------------------------------
bool CreateWindowSurface(U32 resX, U32 resY, bool fullscreen) {
    assert(windowSystemInitialized == true);
    assert(sdlWindow == NULL);

    /* Seed the cached state from the caller's choice so a fullscreen boot is
       fullscreen from the first frame. The create-time property below asks SDL
       to bring the window up fullscreen directly (no windowed flash); the
       post-create apply is kept as a fallback for backends that ignore it. */
    windowFullscreenEnabled = fullscreen;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, resX);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, resY);
    /* Resizable allows OS-driven window resizing on desktop. On Android,
     * SDL translates the resizable flag into SCREEN_ORIENTATION_UNSPECIFIED,
     * which permits portrait rotation and triggers a SDL internal
     * use-after-free on mutex during surfaceChanged. Lock landscape via
     * manifest + non-resizable window. */
#if defined(__ANDROID__)
    /* Android TV: fullscreen at creation avoids a later surface transition
     * (1920×984 → 1920×1080 when system UI hides on focus gain) that
     * triggers EGL_BAD_DISPLAY on some Mali GPUs. Phone: skip the property
     * here to avoid a hwuiTask crash (pthread_mutex_lock on destroyed mutex). */
    if (IsAndroidTVDevice())
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
#else
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, fullscreen);
#endif
    sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (sdlWindow == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to create SDL window.\n"
                  "\tResolution: %ux%u.\n"
                  "\tSDL message: %s",
                  resX, resY, errorMsg);
        return false;
    }

    if (windowFullscreenEnabled && !SDL_SetWindowFullscreen(sdlWindow, true)) {
        const char *errorMsg = SDL_GetError();
        Log_Warn("Unable to switch SDL window to fullscreen.\n"
                 "\tSDL message: %s",
                 errorMsg);
        windowFullscreenEnabled = false;
    }

    sdlRenderer = SDL_CreateRenderer(sdlWindow, NULL);
    if (sdlRenderer == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to create SDL renderer.\n"
                  "\tSDL message: %s",
                  errorMsg);
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = NULL;
        return false;
    }

    SDL_SetRenderVSync(sdlRenderer, 1);

    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, resX, resY);
    if (sdlTexture == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to create SDL streaming texture.\n"
                  "\tSize: %ux%u.\n"
                  "\tSDL message: %s",
                  resX, resY, errorMsg);
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = NULL;
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = NULL;
        return false;
    }

    SDL_SetTextureScaleMode(sdlTexture, SDL_SCALEMODE_NEAREST);
    windowSurfaceResX = resX;
    windowSurfaceResY = resY;

    /* Game draws the pointer in the framebuffer; hide the OS cursor to avoid double cursors. */
    ApplyGameWindowHideSystemCursor();

    return true;
}

bool Window_GetDisplayDimensions(U32 *outW, U32 *outH) {
    if (outW == NULL || outH == NULL || sdlWindow == NULL) {
        return false;
    }
    SDL_DisplayID display = SDL_GetDisplayForWindow(sdlWindow);
    if (display == 0) {
        return false;
    }
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    if (mode == NULL || mode->w <= 0 || mode->h <= 0) {
        return false;
    }
    *outW = (U32)mode->w;
    *outH = (U32)mode->h;
    return true;
}

bool Window_GetPrimaryDisplayDimensions(U32 *outW, U32 *outH) {
    if (outW == NULL || outH == NULL) {
        return false;
    }
    /* Boot-path query: no window exists yet, so go straight to the primary
       display. Requires the video subsystem to be up (the caller brings it
       up before sizing the framebuffer). */
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        return false;
    }
    /* Never auto-detect under the headless drivers. The test harness runs
       with SDL_VIDEODRIVER=dummy and pins --resolution, but guard anyway so a
       stray dummy boot can't pick up the driver's synthetic display mode. */
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver == NULL || SDL_strcmp(driver, "dummy") == 0 ||
        SDL_strcmp(driver, "offscreen") == 0) {
        return false;
    }
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display == 0) {
        return false;
    }
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    if (mode == NULL || mode->w <= 0 || mode->h <= 0) {
        return false;
    }
    *outW = (U32)mode->w;
    *outH = (U32)mode->h;
    return true;
}

bool ResizeWindowSurface(U32 newW, U32 newH) {
    /* No-op if the surface was never created.  Caller should fall through to
       CreateWindowSurface in that case (boot path), not Resize. */
    if (sdlWindow == NULL || sdlRenderer == NULL) {
        return false;
    }

    /* In windowed mode this resizes the visible window; in fullscreen SDL
       ignores the dimension change but the texture below still needs the new
       size so the renderer scales the correct pixel count to fit the
       physical display. */
    SDL_SetWindowSize(sdlWindow, (int)newW, (int)newH);

    /* The streaming texture is fixed-dimension at creation; recreate at the
       new size.  Renderer and window survive untouched, so fullscreen state,
       focus, and multi-monitor placement are preserved. */
    if (sdlTexture) {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = NULL;
    }
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, (int)newW, (int)newH);
    if (sdlTexture == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to recreate SDL streaming texture at %ux%u.\n"
                  "\tSDL message: %s",
                  newW, newH, errorMsg);
        return false;
    }
    SDL_SetTextureScaleMode(sdlTexture, SDL_SCALEMODE_NEAREST);

    windowSurfaceResX = newW;
    windowSurfaceResY = newH;
    return true;
}

void DestroyWindowSurface() {
    if (sdlTexture) {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = NULL;
    }
    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = NULL;
    }
    if (sdlWindow) {
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = NULL;
    }

    windowSurfaceResX = 0;
    windowSurfaceResY = 0;
}

void PresentRendererFrame(const void *pixels, U32 pitch) {
    assert(sdlRenderer != NULL);
    assert(sdlTexture != NULL);
    assert(pixels != NULL);

    SDL_UpdateTexture(sdlTexture, NULL, pixels, (int)pitch);
    PresentCurrentTexture();
}

bool LockRendererTexture(void **pixels, int *pitch) {
    assert(sdlRenderer != NULL);
    assert(sdlTexture != NULL);
    assert(pixels != NULL);
    assert(pitch != NULL);

    return SDL_LockTexture(sdlTexture, NULL, pixels, pitch);
}

void UnlockAndPresentRendererTexture() {
    assert(sdlRenderer != NULL);
    assert(sdlTexture != NULL);

    SDL_UnlockTexture(sdlTexture);
    PresentCurrentTexture();
}

/* Thin SDL-facing wrapper over WindowPointToSurface: supplies the live window
   size. The inverse mapping (and its agreement with the present rect above)
   lives in PRESENT_RECT.CPP. */
void WindowToSurfaceCoords(S32 windowX, S32 windowY, S32 *surfaceX, S32 *surfaceY) {
    int outputW, outputH;
    GetWindowOutputSize(&outputW, &outputH);

    WindowPointToSurface(windowX, windowY, outputW, outputH, windowSurfaceResX,
                         windowSurfaceResY, surfaceX, surfaceY);
}

void SetWindowFullscreen(bool fullscreen) {
    windowFullscreenEnabled = fullscreen;

    if (sdlWindow == NULL) {
        return;
    }

    if (!SDL_SetWindowFullscreen(sdlWindow, fullscreen)) {
        const char *errorMsg = SDL_GetError();
        Log_Warn("Unable to change SDL fullscreen state to %d.\n"
                 "\tSDL message: %s",
                 fullscreen ? 1 : 0, errorMsg);
        windowFullscreenEnabled = !fullscreen;
        return;
    }

    if ((sdlRenderer != NULL) && (sdlTexture != NULL)) {
        PresentCurrentTexture();
    }
}

bool GetWindowFullscreen() {
    return windowFullscreenEnabled;
}

/* The window's *actual* fullscreen state per SDL, rather than the cached request
   flag above (which can disagree when a mode change fails — e.g. SDL refusing to
   leave fullscreen). For accurate reporting / diagnostics. */
bool Window_IsFullscreen(void) {
    return (sdlWindow != NULL) &&
           ((SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_FULLSCREEN) != 0);
}

bool Window_SupportsWindowedMode(void) {
#if defined(__ANDROID__)
    /* Android phones and TVs run the game fullscreen (landscape-locked via
       manifest + non-resizable window, see CreateWindowSurface). There is no
       windowed mode to switch to, so the fullscreen toggle is meaningless. */
    return false;
#else
    return true;
#endif
}

void Window_SetVSync(int enabled) {
    if (sdlRenderer != NULL) {
        SDL_SetRenderVSync(sdlRenderer, enabled ? 1 : 0);
    }
}

void ManageWindow() {
    // Empty
}

void HandleEventsWindow(const void *event) {
    assert(event != NULL);

    // Handle nothing if system was not initialized yet
    if (!windowSystemInitialized) {
        return;
    }

    const SDL_Event *sdlEvent = (SDL_Event *)(event);
    switch (sdlEvent->type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        AppActive = true;
        AppFocusAudio_OnChange(true);
        if (sdlWindow != NULL) {
            ApplyGameWindowHideSystemCursor();
        }
        break;
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        if (sdlWindow != NULL) {
            ApplyGameWindowHideSystemCursor();
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        AppActive = false;
        AppFocusAudio_OnChange(false);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        if ((sdlRenderer != NULL) && (sdlTexture != NULL)) {
            PresentCurrentTexture();
        }
        break;
    case SDL_EVENT_QUIT:
        exit(0); // TODO: Implement graceful exit
        break;
    default:
        break;
    }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
