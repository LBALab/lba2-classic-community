#include <SYSTEM/EVENTS.H>

#include <SVGA/VIDEO.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/WINDOW.H>

#include <SDL3/SDL.h>
#include <assert.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// Implemented in SOURCES/JOYSTICK.CPP. Forward-declared here so LIB386 does
// not pull in game-side headers; the symbol is resolved at final link time.
void HandleEventsJoystick(const void *event);

// --- Private state -----------------------------------------------------------
bool eventSystemInitialized = false;
static EventFilterFn s_eventFilter = NULL;

// --- Initialization ----------------------------------------------------------
bool InitEvents() {
    assert(eventSystemInitialized == false);

    if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) {
        const char *errorMsg = SDL_GetError();
        LogPrintf("Error: Unable to initialize SDL Events subsystem.\n"
                  "\tSDL Message: %s\n",
                  errorMsg);

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
void SetEventFilter(EventFilterFn fn) {
    s_eventFilter = fn;
}

void ManageEvents() {
    assert(eventSystemInitialized == true);

    SDL_PumpEvents();
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            const void *transparentEvent = (void *)(&event);

            if (s_eventFilter && s_eventFilter(transparentEvent))
                continue;

            HandleEventsTimer(transparentEvent);
            HandleEventsMouse(transparentEvent);
            HandleEventsKeyboard(transparentEvent);
            HandleEventsJoystick(transparentEvent);
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
