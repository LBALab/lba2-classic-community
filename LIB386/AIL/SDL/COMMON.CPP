#include "COMMON_SDL.H"

#include "SDK.H"

#include <stdlib.h>

/* The game quits with exit(), which runs the exit handlers and then the static
   destructors of every loaded library while the audio device's callback thread is
   still running. On Android that thread locks a mutex inside the system audio
   library after its destructor has run, and the process aborts.
   Closing audio stops and joins that thread. Registered once audio is up, so it
   runs after the later handlers that stop the music (TheEndInfo) and before the
   destructors of the libraries audio init loaded. */
static void ClearAIL(void) { SDL_QuitSubSystem(SDL_INIT_AUDIO); }

bool AIL_SetHeadless(void) {
    SDL_SetHint(AIL_AUDIO_DRIVER_HINT, "dummy");
    const char *driver = SDL_GetHint(AIL_AUDIO_DRIVER_HINT);
    if (driver == NULL || SDL_strcmp(driver, "dummy") != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Unable to select dummy audio driver");
        return false;
    }
    return true;
}

void InitAIL() {
    /* Success is reported by the boot log's "Audio" line (see InitAdeline);
       only surface a failure here. */
#ifdef LBA2_USE_SDL2
    const bool initialized = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
#else
    const bool initialized = SDL_InitSubSystem(SDL_INIT_AUDIO);
#endif
    if (!initialized) {
        SDL_Log("SDL AIL: Could not init audio: %s", SDL_GetError());
        return;
    }
    atexit(ClearAIL);
}
