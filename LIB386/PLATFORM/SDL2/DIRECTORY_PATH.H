#pragma once

#include <stddef.h>

static inline bool SDL2_DirectorySeparator(char c, bool windows) {
    return c == '/' || (windows && c == '\\');
}

static inline size_t SDL2_DirectoryRootLength(const char *path, bool windows, bool allowDevicePrefix = false) {
    if (allowDevicePrefix) {
        /* Device roots such as sdmc:/ and ux0:/ name mounts, not mkdir targets. */
        size_t end = 0;
        while (path[end] && path[end] != ':' && path[end] != '/')
            ++end;
        if (end && path[end] == ':' && path[end + 1] == '/')
            return end + 2;
    }
    if (windows && SDL2_DirectorySeparator(path[0], windows) &&
        SDL2_DirectorySeparator(path[1], windows)) {
        /* UNC server/share names identify an existing root, not mkdir targets. */
        size_t end = 2;
        while (path[end] && !SDL2_DirectorySeparator(path[end], windows))
            ++end;
        while (SDL2_DirectorySeparator(path[end], windows))
            ++end;
        while (path[end] && !SDL2_DirectorySeparator(path[end], windows))
            ++end;
        return end;
    }
    if (windows && path[0] && path[1] == ':')
        return SDL2_DirectorySeparator(path[2], windows) ? 3 : 2;
    return SDL2_DirectorySeparator(path[0], windows) ? 1 : 0;
}

static inline size_t SDL2_NextDirectoryPrefix(const char *path, size_t *cursor, bool windows) {
    while (SDL2_DirectorySeparator(path[*cursor], windows))
        ++*cursor;
    if (!path[*cursor])
        return 0;
    while (path[*cursor] && !SDL2_DirectorySeparator(path[*cursor], windows))
        ++*cursor;
    return *cursor;
}
