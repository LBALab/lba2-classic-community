#include <SYSTEM/EVENTS.H>

#include <SVGA/VIDEO.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/WINDOW.H>

#include <SDL2/SDL.h>
#include <assert.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Private state -----------------------------------------------------------
bool eventSystemInitialized = false;

// --- Initialization ----------------------------------------------------------
bool InitEvents() {
  assert(eventSystemInitialized == false);

  if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
    const char* errorMsg = SDL_GetError();
    LogPrintf("Error: Unable to initialize SDL Events subsystem.\n"
              "\tSDL Message: %s\n", errorMsg);

    eventSystemInitialized = false;
    return false;
  }

  eventSystemInitialized = true;
  return true;
}

void EndEvents() {
  if (eventSystemInitialized) {
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    eventSystemInitialized = false;
  }
}

// --- Interface ---------------------------------------------------------------
void ManageEvents() {
  assert(eventSystemInitialized == true);

  SDL_PumpEvents();
  {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      const void *transparentEvent = (void *)(&event);

      HandleEventsTimer(transparentEvent);
      HandleEventsMouse(transparentEvent);
      HandleEventsKeyboard(transparentEvent);
      HandleEventsVideo(transparentEvent);
      HandleEventsWindow(transparentEvent);
    }
  }

  // TODO: Pump audio events (refer to WINSYS.CPP)
}

// =============================================================================
#ifdef __cplusplus
}
#endif
