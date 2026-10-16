#include "SWITCH.H"

#include <SDL.h>
#include <stdio.h>
#include <switch.h>

const char *SDL2_SwitchBasePath(void) {
    return "sdmc:/switch/lba2cc/";
}

const char *SDL2_SwitchUserPath(void) {
    return "sdmc:/switch/lba2cc/user/";
}

void SDL2_SwitchWindowSize(int *width, int *height) {
    const bool handheld = appletGetOperationMode() == AppletOperationMode_Handheld;
    *width = handheld ? 1280 : 1920;
    *height = handheld ? 720 : 1080;
}

bool SDL2_SwitchMessageBox(const char *title, const char *message) {
    char body[2048];
    const int length = snprintf(body, sizeof(body),
                                "%s\n\n%s\n\nGame files: sdmc:/switch/lba2cc/data/\n"
                                "Settings, saves and logs: sdmc:/switch/lba2cc/user/\n"
                                "See README.txt for setup help.",
                                title, message);
    if (length < 0 || (size_t)length >= sizeof(body)) {
        SDL_SetError("Switch message is too long");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    ErrorApplicationConfig config;
    Result result = errorApplicationCreate(&config, title, body);
    if (R_SUCCEEDED(result))
        result = errorApplicationShow(&config);
    if (R_FAILED(result)) {
        SDL_SetError("Unable to show Switch message: 0x%08x", (unsigned int)result);
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    return true;
}
