#include "PRIVATE.H"

#include <stdio.h>
#include <stdlib.h>

int Lba2Main(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    SDL_SetMainReady();
    /* The game also exits through exit(), so return-only teardown misses it. */
    if (atexit(g_platform.system.shutdown) != 0) {
        fprintf(stderr, "Unable to register Switch platform shutdown\n");
        return EXIT_FAILURE;
    }
    return Lba2Main(argc, argv);
}
