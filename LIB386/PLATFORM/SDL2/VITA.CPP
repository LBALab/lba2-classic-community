#include "PRIVATE.H"
#include "VITA.H"

bool SDL2_VitaInitGameData(void) {
    const char *path = SDL_getenv("LBA2_GAME_DIR");
    if (path && *path)
        return true;
    if (SDL_setenv("LBA2_GAME_DIR", "ux0:/data/lba2/", 1) != 0) {
        SDL_SetError("Unable to set the Vita game-data directory");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    return true;
}

const char *SDL2_VitaBasePath(void) {
    return "app0:/";
}

const char *SDL2_VitaUserPath(void) {
    return "ux0:/data/lba2cc/user/";
}

bool SDL2_VitaConfigureWindow(int *width, int *height) {
    /* GXM presents without an OpenGL context or external shader plugins. */
    if (!SDL_SetHint(SDL_HINT_RENDER_DRIVER, "VITA gxm")) {
        SDL_SetError("Unable to select the Vita GXM renderer");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    *width = 960;
    *height = 544;
    return true;
}
