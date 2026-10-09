#include <PLATFORM/PLATFORM.H>

#include <stdarg.h>
#include <stdio.h>

namespace {
PlatformLogFn s_logOutput = NULL;
void *s_logUserdata = NULL;
} // namespace

extern "C" {

void Platform_SetLogOutput(PlatformLogFn callback, void *userdata) {
    s_logOutput = callback;
    s_logUserdata = userdata;
    g_platform.system.set_log_output(callback, userdata);
}

void Platform_Log(PlatformLogLevel level, const char *fmt, ...) {
    char message[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    if (s_logOutput) {
        s_logOutput(s_logUserdata, level, message);
    } else {
        static const char *tags[] = {"DEBUG", "INFO", "WARN", "ERROR"};
        fprintf(stderr, "[%s] %s\n", tags[level], message);
    }
}

bool Platform_ConvertIndexedFrame(const PlatformFrame *frame, uint32_t *pixels, uint32_t pitch) {
    if (frame == NULL || frame->pixels == NULL || frame->palette_rgb == NULL ||
        pixels == NULL || frame->width == 0 || frame->height == 0 ||
        frame->pitch < frame->width || pitch % sizeof(uint32_t) != 0 ||
        pitch / sizeof(uint32_t) < frame->width) {
        Platform_Log(PLATFORM_LOG_ERROR, "Invalid indexed frame or presentation pitch");
        return false;
    }

    uint32_t lut[256];
    for (uint32_t i = 0; i < 256; ++i) {
        const uint8_t *rgb = frame->palette_rgb + i * 3;
        lut[i] = (uint32_t)0xFF000000 | ((uint32_t)rgb[0] << 16) |
                 ((uint32_t)rgb[1] << 8) | rgb[2];
    }
    for (uint32_t y = 0; y < frame->height; ++y) {
        const uint8_t *src = frame->pixels + (size_t)y * frame->pitch;
        uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (size_t)y * pitch);
        for (uint32_t x = 0; x < frame->width; ++x)
            dst[x] = lut[src[x]];
    }
    return true;
}
}
