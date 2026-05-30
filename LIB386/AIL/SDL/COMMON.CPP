#include "COMMON_SDL.H"

#include "SDL3/SDL.h"

void InitAIL() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("SDL AIL: Audio subsystem initialized");
    } else {
        SDL_Log("SDL AIL: Could not init audio: %s", SDL_GetError());
    }
}
