#include <SYSTEM/WINDOW.H>

#include <SYSTEM/LOGPRINT.H>

#include <SDL2/SDL.h>
#include <assert.h>

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

  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
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

  sdlWindow = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, resX, resY, 0);
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

  int ret = SDL_UpdateWindowSurfaceRects(sdlWindow, &updateRect, areaCount);
  if (ret != 0) {
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

  if (sdlEvent->type == SDL_WINDOWEVENT) {
    switch (sdlEvent->window.event) {
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      AppActive = true;
      break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
      AppActive = false;
      break;
    }
  } else if (sdlEvent->type == SDL_QUIT) {
    exit(0); // TODO: Implement graceful exit
  }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
