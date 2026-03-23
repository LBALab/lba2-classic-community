#include <SYSTEM/WINDOW.H>

#include <SYSTEM/LOGPRINT.H>

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

static void ComputePresentationRect(SDL_FRect *dstRect) {
    assert(dstRect != NULL);

    dstRect->x = 0.0f;
    dstRect->y = 0.0f;
    dstRect->w = (float)windowSurfaceResX;
    dstRect->h = (float)windowSurfaceResY;

    if ((sdlWindow == NULL) || (windowSurfaceResX == 0) || (windowSurfaceResY == 0)) {
        return;
    }

    int outputW = (int)windowSurfaceResX;
    int outputH = (int)windowSurfaceResY;
    SDL_GetWindowSize(sdlWindow, &outputW, &outputH);
    if ((outputW <= 0) || (outputH <= 0)) {
        return;
    }

    const double surfaceAspect = (double)windowSurfaceResX / (double)windowSurfaceResY;
    const double outputAspect = (double)outputW / (double)outputH;

    if (outputAspect > surfaceAspect) {
        dstRect->h = (float)outputH;
        dstRect->w = (float)(dstRect->h * surfaceAspect);
        dstRect->x = ((float)outputW - dstRect->w) * 0.5f;
        dstRect->y = 0.0f;
    } else {
        dstRect->w = (float)outputW;
        dstRect->h = (float)(dstRect->w / surfaceAspect);
        dstRect->x = 0.0f;
        dstRect->y = ((float)outputH - dstRect->h) * 0.5f;
    }
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
        LogPrintf("Error: Unable to initialize SDL Window/Video subsystem.\n"
                  "\tSDL Message: %s\n",
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
bool CreateWindowSurface(U32 resX, U32 resY) {
    assert(windowSystemInitialized == true);
    assert(sdlWindow == NULL);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, resX);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, resY);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (sdlWindow == NULL) {
        const char *errorMsg = SDL_GetError();
        LogPrintf("Error: Unable to create SDL window.\n"
                  "\tResolution: %ux%u.\n"
                  "\tSDL message: %s\n",
                  resX, resY, errorMsg);
        return false;
    }

    if (windowFullscreenEnabled && !SDL_SetWindowFullscreen(sdlWindow, true)) {
        const char *errorMsg = SDL_GetError();
        LogPrintf("Warning: Unable to switch SDL window to fullscreen.\n"
                  "\tSDL message: %s\n",
                  errorMsg);
        windowFullscreenEnabled = false;
    }

    sdlRenderer = SDL_CreateRenderer(sdlWindow, NULL);
    if (sdlRenderer == NULL) {
        const char *errorMsg = SDL_GetError();
        LogPrintf("Error: Unable to create SDL renderer.\n"
                  "\tSDL message: %s\n",
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
        LogPrintf("Error: Unable to create SDL streaming texture.\n"
                  "\tSize: %ux%u.\n"
                  "\tSDL message: %s\n",
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

void WindowToSurfaceCoords(S32 windowX, S32 windowY, S32 *surfaceX, S32 *surfaceY) {
    assert(surfaceX != NULL);
    assert(surfaceY != NULL);

    if ((windowSurfaceResX == 0) || (windowSurfaceResY == 0)) {
        *surfaceX = windowX;
        *surfaceY = windowY;
        return;
    }

    SDL_FRect dstRect;
    float relX;
    float relY;

    ComputePresentationRect(&dstRect);

    if ((dstRect.w <= 0.0f) || (dstRect.h <= 0.0f)) {
        *surfaceX = windowX;
        *surfaceY = windowY;
        return;
    }

    relX = (float)windowX - dstRect.x;
    relY = (float)windowY - dstRect.y;

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

    *surfaceX = (S32)(relX * ((float)windowSurfaceResX / dstRect.w));
    *surfaceY = (S32)(relY * ((float)windowSurfaceResY / dstRect.h));

    if (*surfaceX >= (S32)windowSurfaceResX) {
        *surfaceX = (S32)windowSurfaceResX - 1;
    }
    if (*surfaceY >= (S32)windowSurfaceResY) {
        *surfaceY = (S32)windowSurfaceResY - 1;
    }
}

void SetWindowFullscreen(bool fullscreen) {
    windowFullscreenEnabled = fullscreen;

    if (sdlWindow == NULL) {
        return;
    }

    if (!SDL_SetWindowFullscreen(sdlWindow, fullscreen)) {
        const char *errorMsg = SDL_GetError();
        LogPrintf("Warning: Unable to change SDL fullscreen state to %d.\n"
                  "\tSDL message: %s\n",
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
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        AppActive = false;
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
