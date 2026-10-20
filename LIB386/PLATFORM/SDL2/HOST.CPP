#include "PRIVATE.H"
#include "DIRECTORY_PATH.H"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {
#ifdef LBA2_SDL2_TESTS
int (*s_messageProbe)(Uint32, const char *, const char *, SDL_Window *) = NULL;
int (*s_mkdirProbe)(const char *) = NULL;
#endif
bool HostError(const char *operation, const char *path = NULL) {
    if (path)
        SDL_SetError("%s '%s': %s", operation, path, strerror(errno));
    else
        SDL_SetError("%s: %s", operation, strerror(errno));
    Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
    return false;
}

bool Unavailable() {
    SDL_SetError("SDL2 has no native folder-dialog service; use --game-dir");
    Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
    return false;
}
} // namespace

#ifdef LBA2_SDL2_TESTS
void SDL2_SetMessageProbe(int (*probe)(Uint32, const char *, const char *, SDL_Window *)) {
    s_messageProbe = probe;
}
void SDL2_SetMkdirProbe(int (*probe)(const char *)) {
    s_mkdirProbe = probe;
}
#endif

char *SDL2_CurrentDirectory() {
    char path[4096];
#ifdef _WIN32
    const bool ok = _getcwd(path, sizeof(path)) != NULL;
#else
    const bool ok = getcwd(path, sizeof(path)) != NULL;
#endif
    if (!ok) {
        HostError("getcwd");
        return NULL;
    }
    char *copy = SDL_strdup(path);
    if (!copy)
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
    return copy;
}
const char *SDL2_HomePath() {
#ifdef __SWITCH__
    const char *home = SDL2_SwitchBasePath();
#elif defined(__vita__)
    const char *home = SDL2_VitaUserPath();
#elif defined(_WIN32)
    const char *home = SDL_getenv("USERPROFILE");
#else
    const char *home = SDL_getenv("HOME");
#endif
    if (!home || !*home) {
        SDL_SetError("Home directory is unavailable");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return NULL;
    }
    return home;
}
bool SDL2_CreateDirectory(const char *path) {
    if (!path || !*path) {
        SDL_SetError("Host directory path is empty");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    char *copy = SDL_strdup(path);
    if (!copy) {
        SDL_OutOfMemory();
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
#ifdef _WIN32
    const bool windows = true;
#else
    const bool windows = false;
#endif
#if defined(__SWITCH__) || defined(__vita__)
    const bool allowDevicePrefix = true;
#else
    const bool allowDevicePrefix = false;
#endif
    size_t cursor = SDL2_DirectoryRootLength(copy, windows, allowDevicePrefix);
    size_t i = SDL2_NextDirectoryPrefix(copy, &cursor, windows);
    bool ok = true;
    if (!i) {
        struct stat info;
        if (stat(copy, &info) != 0)
            ok = HostError("stat", copy);
        else if ((info.st_mode & S_IFMT) != S_IFDIR) {
            errno = ENOTDIR;
            ok = HostError("stat", copy);
        }
    }
    while (ok && i != 0) {
        const char saved = copy[i];
        copy[i] = '\0';
        struct stat info;
        if (stat(copy, &info) == 0) {
            /* A readable parent need not grant permission to create it again. */
            if ((info.st_mode & S_IFMT) != S_IFDIR) {
                errno = ENOTDIR;
                ok = HostError("mkdir", copy);
            }
        } else {
            int result;
#ifdef LBA2_SDL2_TESTS
            if (s_mkdirProbe)
                result = s_mkdirProbe(copy);
            else
#endif
            {
#ifdef _WIN32
                result = _mkdir(copy);
#else
                result = mkdir(copy, 0770);
#endif
            }
            if (result != 0) {
                const int creationError = errno;
                if (stat(copy, &info) != 0 || (info.st_mode & S_IFMT) != S_IFDIR) {
                    errno = creationError;
                    ok = HostError("mkdir", copy);
                }
            }
        }
        copy[i] = saved;
        i = SDL2_NextDirectoryPrefix(copy, &cursor, windows);
    }
    SDL_free(copy);
    return ok;
}
bool SDL2_RemovePath(const char *path) {
    if (!path || !*path) {
        SDL_SetError("Host removal path is empty");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    return remove(path) == 0 || errno == ENOENT || HostError("remove");
}
bool SDL2_MessageBox(PlatformMessageKind kind, const char *title, const char *message) {
#ifdef __SWITCH__
    return SDL2_SwitchMessageBox(title, message);
#else
    if (!SDL2_InitVideo())
        return false;
    const Uint32 flags = kind == PLATFORM_MESSAGE_ERROR ? SDL_MESSAGEBOX_ERROR : kind == PLATFORM_MESSAGE_WARNING ? SDL_MESSAGEBOX_WARNING
                                                                                                                  : SDL_MESSAGEBOX_INFORMATION;
    int result;
#ifdef LBA2_SDL2_TESTS
    if (s_messageProbe)
        result = s_messageProbe(flags, title, message, NULL);
    else
#endif
        result = SDL_ShowSimpleMessageBox(flags, title, message, NULL);
    if (result == 0)
        return true;
    Platform_Log(PLATFORM_LOG_WARN, "Unable to show SDL2 message box: %s", SDL_GetError());
    return false;
#endif
}
PlatformFolderDialog *SDL2_BeginFolderDialog(const char *) {
    Unavailable();
    return NULL;
}
PlatformDialogResult SDL2_PollFolderDialog(PlatformFolderDialog *, const char **path) {
    if (path)
        *path = NULL;
    Unavailable();
    return PLATFORM_DIALOG_ERROR;
}
void SDL2_EndFolderDialog(PlatformFolderDialog *dialog) {
    if (dialog)
        Unavailable();
}
