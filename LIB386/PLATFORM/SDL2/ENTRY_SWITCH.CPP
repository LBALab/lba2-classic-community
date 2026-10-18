#include "PRIVATE.H"

#include <stdio.h>
#include <stdlib.h>

int Lba2Main(int argc, char *argv[]);

namespace {
void Shutdown() {
    g_platform.system.shutdown();
    SDL2_SwitchEndNetloader();
}
} // namespace

int main(int argc, char *argv[]) {
    SDL_SetMainReady();
    /* The game also exits through exit(), so return-only teardown misses it. */
    if (atexit(Shutdown) != 0) {
        fprintf(stderr, "Unable to register Switch platform shutdown\n");
        return EXIT_FAILURE;
    }
    if (!SDL2_SwitchInitGameData()) {
        SDL2_SwitchMessageBox("Game data setup failed", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (!SDL2_SwitchInitNetloader())
        SDL2_SwitchMessageBox("Network logging unavailable", SDL_GetError());
    return Lba2Main(argc, argv);
}
