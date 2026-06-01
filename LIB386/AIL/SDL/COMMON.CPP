#include "COMMON_SDL.H"

#include "SDL3/SDL.h"

void InitAIL() {
    /* Success is reported by the boot log's "Audio" line (see InitAdeline);
       only surface a failure here. */
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("SDL AIL: Could not init audio: %s", SDL_GetError());
    }
}
