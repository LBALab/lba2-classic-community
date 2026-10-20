#include "PRIVATE.H"
#ifdef LBA2_VITA_DIAGNOSTICS
#include "VITA_LOG.H"
#endif

#include <stdio.h>
#include <stdlib.h>

int Lba2Main(int argc, char *argv[]);

/* The default newlib heap is too small for the engine's resource caches. */
extern "C" {
int _newlib_heap_size_user = 128 * 1024 * 1024;
}

namespace {
void Shutdown() {
#ifdef LBA2_VITA_DIAGNOSTICS
    fprintf(stderr, "[Vita boot] Platform shutdown\n");
#endif
    g_platform.system.shutdown();
#ifdef LBA2_VITA_DIAGNOSTICS
    SDL2_VitaEndDiagnostics();
#endif
}
} // namespace

int main(int argc, char *argv[]) {
#ifdef LBA2_VITA_DIAGNOSTICS
    if (!SDL2_VitaInitDiagnostics("ux0:/data/lba2cc-boot.log", "ux0:/data/lba2cc-debugnet.txt")) {
        SDL_SetMainReady();
        SDL2_MessageBox(PLATFORM_MESSAGE_ERROR, "Diagnostic setup failed",
                        "Could not create ux0:/data/lba2cc-boot.log. Check writable storage.");
        return EXIT_FAILURE;
    }
#endif
    SDL_SetMainReady();
    if (atexit(Shutdown) != 0) {
        fprintf(stderr, "Unable to register Vita platform shutdown\n");
#ifdef LBA2_VITA_DIAGNOSTICS
        SDL2_VitaEndDiagnostics();
#endif
        return EXIT_FAILURE;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (!SDL2_VitaInitGameData()) {
        SDL2_MessageBox(PLATFORM_MESSAGE_ERROR, "Game data setup failed", SDL_GetError());
        return EXIT_FAILURE;
    }
#ifdef LBA2_VITA_DIAGNOSTICS
    fprintf(stderr, "[Vita boot] Calling Lba2Main: argc=%d\n", argc);
#endif
    const int result = Lba2Main(argc, argv);
#ifdef LBA2_VITA_DIAGNOSTICS
    fprintf(stderr, "[Vita boot] Lba2Main returned %d\n", result);
#endif
    return result;
}
