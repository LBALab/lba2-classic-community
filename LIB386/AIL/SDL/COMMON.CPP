#include "COMMON_SDL.H"

#include <SYSTEM/EXIT.H>

#include "SDL3/SDL.h"

void ClearAIL(void);

void InitAIL() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("SDL AIL: Audio subsystem initialized");
    } else {
        SDL_Log("SDL AIL: Could not init audio: %s", SDL_GetError());
    }
    RegisterInitClear(InitAIL, ClearAIL);
}

void ClearAIL() {
    /* Shut down the SDL audio subsystem. This cleans up all audio
       streams and devices. On Android this must not race with the
       app lifecycle — atexit handlers run at process exit only, so
       it is safe here. */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
