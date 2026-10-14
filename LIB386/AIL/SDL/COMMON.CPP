#include "COMMON_SDL.H"

#include "SDK.H"

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
    }
}
