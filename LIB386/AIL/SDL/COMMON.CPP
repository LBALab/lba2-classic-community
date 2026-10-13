#include "COMMON_SDL.H"

#include "SDL3/SDL.h"

bool AIL_SetHeadless(void) {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    const char *driver = SDL_GetHint(SDL_HINT_AUDIO_DRIVER);
    if (driver == NULL || SDL_strcmp(driver, "dummy") != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Unable to select dummy audio driver");
        return false;
    }
    return true;
}

void InitAIL() {
    /* Success is reported by the boot log's "Audio" line (see InitAdeline);
       only surface a failure here. */
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("SDL AIL: Could not init audio: %s", SDL_GetError());
    }
}
