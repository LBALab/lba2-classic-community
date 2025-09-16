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

  return true;
}

void *WindowSurface() {
  assert(windowSystemInitialized == true);
  assert(sdlWindow != NULL);

  SDL_Surface *sdlSurface = SDL_GetWindowSurface(sdlWindow);
  if (sdlSurface == NULL) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Error: Unable to create SDL video surface.\n"
              "\tSDL message: %s\n",
              errorMsg);
  }

  return sdlSurface;
}

void DestroyWindowSurface() {
  if (sdlWindow) {
    SDL_DestroyWindow(sdlWindow);
    sdlWindow = NULL;
  }
}

void UpdateWindowSurface() {
  int ret = SDL_UpdateWindowSurface(sdlWindow);
  if (ret != 0) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Warning: Unable to update SDL window surface entire area.\n"
              "\tSDL message: %s\n",
              errorMsg);
  }
}

void UpdateWindowSurfaceArea(const T_BOX area[], U32 areaCount) {
  assert(area != NULL);
  assert(areaCount > 0);
  assert(areaCount == 1 && "Support for many areas per update not implemented");

  SDL_Rect updateRect = {area->x0, area->y0,
                         (area->x1 - area->x0),  // Width
                         (area->y1 - area->y0)}; // Height

  if (!SDL_UpdateWindowSurfaceRects(sdlWindow, &updateRect, areaCount)) {
    const char *errorMsg = SDL_GetError();
    LogPrintf("Warning: Unable to update SDL window surface area.\n"
              "\tRect: Origin: [%i, %i] - Size: [%i, %i]\n"
              "\tSDL message: %s\n",
              updateRect.x, updateRect.y, updateRect.w, updateRect.h, errorMsg);
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
