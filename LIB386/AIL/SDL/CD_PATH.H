#pragma once

#include <stdio.h>
#include <string.h>

static inline bool SDLCD_FormatTrackPath(char *out, int capacity, const char *directory,
                                         const char *musicDirectory, int track) {
    if (!out || capacity <= 0 || !directory || !*directory || !musicDirectory) {
        if (out && capacity > 0)
            out[0] = '\0';
        return false;
    }
    const char last = directory[strlen(directory) - 1];
    const char *separator = last == '/' || last == '\\' ? "" : "/";
    const int length = snprintf(out, capacity, "%s%s%s/Track%02d.wav",
                                directory, separator, musicDirectory, track);
    if (length < 0 || length >= capacity) {
        out[0] = '\0';
        return false;
    }
    return true;
}
