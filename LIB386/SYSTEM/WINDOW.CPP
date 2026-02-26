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
}

void PresentRendererFrame(const void *pixels, U32 pitch) {
  assert(sdlRenderer != NULL);
  assert(sdlTexture != NULL);
  assert(pixels != NULL);

  SDL_UpdateTexture(sdlTexture, NULL, pixels, (int)pitch);

  SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
  SDL_RenderClear(sdlRenderer);
  SDL_RenderTexture(sdlRenderer, sdlTexture, NULL, NULL);
  SDL_RenderPresent(sdlRenderer);
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
