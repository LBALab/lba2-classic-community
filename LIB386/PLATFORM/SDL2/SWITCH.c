#include "SWITCH.H"

#include <SDL.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>
#include <unistd.h>

static int s_nxlinkSocket = -1;
static int s_savedStdout = -1;
static int s_savedStderr = -1;

static void RestoreOutput(int *saved, int output) {
    if (*saved < 0)
        return;
    if (dup2(*saved, output) < 0) {
        const int error = errno;
        if (close(output) < 0 && errno != EBADF)
            Platform_Log(PLATFORM_LOG_WARN, "Unable to close Switch output %d: %s", output, strerror(errno));
        Platform_Log(PLATFORM_LOG_WARN, "Unable to restore Switch output %d: %s", output, strerror(error));
    }
    if (close(*saved) < 0)
        Platform_Log(PLATFORM_LOG_WARN, "Unable to close saved Switch output %d: %s", output, strerror(errno));
    *saved = -1;
}

bool SDL2_SwitchInitGameData(void) {
    const char *path = SDL_getenv("LBA2_GAME_DIR");
    if (path && *path)
        return true;
    if (SDL_setenv("LBA2_GAME_DIR", "sdmc:/lba2/", 1) != 0) {
        SDL_SetError("Unable to set the Switch game-data directory");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    return true;
}

bool SDL2_SwitchInitNetloader(void) {
    if (!__nxlink_host.s_addr || s_nxlinkSocket >= 0)
        return true;
    const Result result = socketInitializeDefault();
    if (R_FAILED(result)) {
        SDL_SetError("Unable to initialize Switch network logging: 0x%08x", (unsigned int)result);
        Platform_Log(PLATFORM_LOG_WARN, "%s", SDL_GetError());
        return false;
    }
    s_savedStdout = dup(STDOUT_FILENO);
    if (s_savedStdout >= 0)
        s_savedStderr = dup(STDERR_FILENO);
    if (s_savedStdout < 0 || s_savedStderr < 0) {
        SDL_SetError("Unable to preserve Switch standard output: %s", strerror(errno));
        RestoreOutput(&s_savedStdout, STDOUT_FILENO);
        RestoreOutput(&s_savedStderr, STDERR_FILENO);
        socketExit();
        Platform_Log(PLATFORM_LOG_WARN, "%s", SDL_GetError());
        return false;
    }
    s_nxlinkSocket = nxlinkStdio();
    if (s_nxlinkSocket < 0) {
        SDL_SetError("Unable to connect Switch network logging: %s", strerror(errno));
        RestoreOutput(&s_savedStdout, STDOUT_FILENO);
        RestoreOutput(&s_savedStderr, STDERR_FILENO);
        socketExit();
        Platform_Log(PLATFORM_LOG_WARN, "%s", SDL_GetError());
        return false;
    }
    return true;
}

void SDL2_SwitchEndNetloader(void) {
    if (s_nxlinkSocket < 0)
        return;
    fflush(stdout);
    fflush(stderr);
    /* libc closes stdio after atexit; socketExit removes its device table. */
    RestoreOutput(&s_savedStdout, STDOUT_FILENO);
    RestoreOutput(&s_savedStderr, STDERR_FILENO);
    clearerr(stdout);
    clearerr(stderr);
    if (close(s_nxlinkSocket) < 0)
        Platform_Log(PLATFORM_LOG_WARN, "Unable to close Switch network logging: %s", strerror(errno));
    s_nxlinkSocket = -1;
    socketExit();
}

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
                                "%s\n\n%s\n\nGame files: sdmc:/lba2/\n"
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
