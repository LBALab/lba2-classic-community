#include "LOG.H"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef LBA_LOG_NO_SDL
#include <SDL3/SDL_log.h>
/* App-defined SDL log categories: one for ordinary records, one to tag section
 * headers so the trampoline can recover is-header without parsing the text. */
#define LBA_CAT_LOG (SDL_LOG_CATEGORY_CUSTOM + 0)
#define LBA_CAT_SECTION (SDL_LOG_CATEGORY_CUSTOM + 1)
#endif

#define LOG_MSG_MAX 1024
#define LOG_MAX_SINKS 8

/* Internals in an anonymous namespace for internal linkage — the house
 * new-infrastructure style (see SOURCES/RES_PICKER.CPP). */
namespace {

typedef void (*SinkWriteFn)(LogSink *sink, LogSeverity sev, int is_header,
                            const char *msg);

} // namespace

/* LogSink is declared opaque in the header; the definition lives here so the
 * fixed-size pool can hold values rather than pointers-to-heap. */
struct LogSink {
    int in_use;
    LogSeverity min_severity;
    SinkWriteFn write;
    FILE *fp; /* file sink only */
};

namespace {

LogSink g_pool[LOG_MAX_SINKS];    /* storage */
LogSink *g_active[LOG_MAX_SINKS]; /* dispatch list */
int g_active_count = 0;

const char *severity_tag(LogSeverity sev) {
    switch (sev) {
    case LOG_DEBUG:
        return "DEBUG";
    case LOG_INFO:
        return "INFO";
    case LOG_WARN:
        return "WARN";
    case LOG_ERROR:
        return "ERROR";
    }
    return "INFO";
}

/* Fan-out: write the record to every active sink that admits this severity. */
void log_emit(LogSeverity sev, int is_header, const char *msg) {
    for (int i = 0; i < g_active_count; ++i) {
        LogSink *s = g_active[i];
        if (sev < s->min_severity)
            continue;
        s->write(s, sev, is_header, msg);
    }
}

void file_sink_write(LogSink *s, LogSeverity sev, int is_header,
                     const char *msg) {
    if (!s->fp)
        return;
    if (is_header)
        fprintf(s->fp, "==== %s ====\n", msg);
    else
        fprintf(s->fp, "[%s] %s\n", severity_tag(sev), msg);
    fflush(s->fp);
}

LogSink *alloc_sink(void) {
    for (int i = 0; i < LOG_MAX_SINKS; ++i) {
        if (!g_pool[i].in_use) {
            LogSink *s = &g_pool[i];
            s->in_use = 1;
            s->min_severity = LOG_DEBUG;
            s->write = 0;
            s->fp = 0;
            return s;
        }
    }
    return 0; /* pool exhausted — drop silently (logging must never abort) */
}

#ifndef LBA_LOG_NO_SDL
SDL_LogPriority sev_to_sdl(LogSeverity sev) {
    switch (sev) {
    case LOG_DEBUG:
        return SDL_LOG_PRIORITY_DEBUG;
    case LOG_INFO:
        return SDL_LOG_PRIORITY_INFO;
    case LOG_WARN:
        return SDL_LOG_PRIORITY_WARN;
    case LOG_ERROR:
        return SDL_LOG_PRIORITY_ERROR;
    }
    return SDL_LOG_PRIORITY_INFO;
}

LogSeverity sdl_to_sev(SDL_LogPriority pri) {
    switch (pri) {
    case SDL_LOG_PRIORITY_WARN:
        return LOG_WARN;
    case SDL_LOG_PRIORITY_ERROR:
    case SDL_LOG_PRIORITY_CRITICAL:
        return LOG_ERROR;
    case SDL_LOG_PRIORITY_INFO:
        return LOG_INFO;
    default:
        return LOG_DEBUG; /* TRACE / VERBOSE / DEBUG */
    }
}

/* The single fan-out point. Installing this also routes the engine's existing
 * SDL_Log sites and SDL's own messages through our sinks (see the plan). */
void SDLCALL sdl_output(void *userdata, int category, SDL_LogPriority pri,
                        const char *message) {
    (void)userdata;
    log_emit(sdl_to_sev(pri), category == LBA_CAT_SECTION, message);
}
#endif /* !LBA_LOG_NO_SDL */

/* Transport seam: format-then-dispatch. Under SDL the record rides the log
 * spine and comes back through sdl_output; in host tests it goes straight to
 * the fan-out, so the core needs no SDL_Init. */
void emit_formatted(LogSeverity sev, int is_header, const char *msg) {
#ifdef LBA_LOG_NO_SDL
    log_emit(sev, is_header, msg);
#else
    SDL_LogMessage(is_header ? LBA_CAT_SECTION : LBA_CAT_LOG, sev_to_sdl(sev),
                   "%s", msg);
#endif
}

void emit_va(LogSeverity sev, const char *fmt, va_list ap) {
    char buf[LOG_MSG_MAX];
    (void)vsnprintf(buf, sizeof buf, fmt, ap);
    emit_formatted(sev, 0, buf);
}

} // namespace

extern "C" {

void Log_Init(void) {
    g_active_count = 0;
    for (int i = 0; i < LOG_MAX_SINKS; ++i)
        g_pool[i].in_use = 0;
#ifndef LBA_LOG_NO_SDL
    SDL_SetLogOutputFunction(sdl_output, 0);
#endif
}

void Log_Shutdown(void) {
#ifndef LBA_LOG_NO_SDL
    SDL_SetLogOutputFunction(0, 0);
#endif
    for (int i = 0; i < LOG_MAX_SINKS; ++i) {
        if (g_pool[i].in_use && g_pool[i].fp) {
            fclose(g_pool[i].fp);
            g_pool[i].fp = 0;
        }
        g_pool[i].in_use = 0;
    }
    g_active_count = 0;
}

void Log_Debug(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit_va(LOG_DEBUG, fmt, ap);
    va_end(ap);
}

void Log_Info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit_va(LOG_INFO, fmt, ap);
    va_end(ap);
}

void Log_Warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit_va(LOG_WARN, fmt, ap);
    va_end(ap);
}

void Log_Error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit_va(LOG_ERROR, fmt, ap);
    va_end(ap);
}

void Log_BeginSection(const char *title) {
    emit_formatted(LOG_INFO, 1, title ? title : "");
}

void Log_EndSection(void) {
    /* No file-sink output; bracketing sinks (later phases) hook here. */
}

LogSink *Log_MakeFileSink(const char *path, LogSeverity min_severity) {
    if (!path)
        return 0;
    LogSink *s = alloc_sink();
    if (!s)
        return 0;
    s->fp = fopen(path, "a");
    if (!s->fp) {
        s->in_use = 0;
        return 0;
    }
    s->min_severity = min_severity;
    s->write = file_sink_write;
    return s;
}

void Log_AddSink(LogSink *sink) {
    if (!sink || g_active_count >= LOG_MAX_SINKS)
        return;
    for (int i = 0; i < g_active_count; ++i)
        if (g_active[i] == sink)
            return; /* already active */
    g_active[g_active_count++] = sink;
}

void Log_RemoveSink(LogSink *sink) {
    for (int i = 0; i < g_active_count; ++i) {
        if (g_active[i] == sink) {
            for (int j = i; j < g_active_count - 1; ++j)
                g_active[j] = g_active[j + 1];
            --g_active_count;
            return;
        }
    }
}

} // extern "C"
