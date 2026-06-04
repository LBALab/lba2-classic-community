#include <SYSTEM/LOG.H>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LBA_LOG_NO_SDL
#include <SDL3/SDL_thread.h> /* main-thread gate for the console sink */
#endif

#if defined(_WIN32)
#include <io.h>      /* _isatty, _fileno */
#include <windows.h> /* GetStdHandle, SetConsoleMode (VT enable) */
#else
#include <unistd.h> /* isatty, fileno */
#endif

#ifndef LBA_LOG_NO_SDL
#include <SDL3/SDL_log.h>
/* App-defined SDL log categories: ordinary records, section begin, section end —
 * so the trampoline recovers the record kind without parsing the text. */
#define LBA_CAT_LOG (SDL_LOG_CATEGORY_CUSTOM + 0)
#define LBA_CAT_SECTION (SDL_LOG_CATEGORY_CUSTOM + 1)
#define LBA_CAT_SECTION_END (SDL_LOG_CATEGORY_CUSTOM + 2)
#define LBA_CAT_RAW (SDL_LOG_CATEGORY_CUSTOM + 3)
#define LBA_CAT_BANNER (SDL_LOG_CATEGORY_CUSTOM + 4)
#endif

#define LOG_MSG_MAX 1024
#define LOG_MAX_SINKS 8
#define LOG_TERM_WIDTH 60 /* section rule width for the terminal sink */

/* Internals in an anonymous namespace for internal linkage — the house
 * new-infrastructure style (see SOURCES/RES_PICKER.CPP). */
namespace {

/* Record kind, carried alongside severity through the fan-out. */
enum { REC_NORMAL = 0,
       REC_SECTION_BEGIN = 1,
       REC_SECTION_END = 2,
       REC_RAW = 3,
       REC_BANNER = 4 };

typedef void (*SinkWriteFn)(LogSink *sink, LogSeverity sev, int kind,
                            const char *msg);

} // namespace

/* LogSink is declared opaque in the header; the definition lives here so the
 * fixed-size pool can hold values rather than pointers-to-heap. */
struct LogSink {
    int in_use;
    LogSeverity min_severity;
    SinkWriteFn write;
    FILE *fp;    /* file sink: owned; terminal sink: stderr (not owned) */
    int owns_fp; /* close fp on shutdown (file sink only) */
    /* console sink: per-line print adapter, injected by the engine */
    LogLineFn line_fn;
};

namespace {

LogSink g_pool[LOG_MAX_SINKS];    /* storage */
LogSink *g_active[LOG_MAX_SINKS]; /* dispatch list */
int g_active_count = 0;

/* The one console sink, if registered — so Log_SetConsoleSeverity can find it
 * without the caller threading the handle back through. */
LogSink *g_console_sink = 0;

#ifndef LBA_LOG_NO_SDL
/* Captured in Log_Init (on the main thread). The console sink drops records
 * from any other thread; the ring is not thread-safe. */
SDL_ThreadID g_main_thread = 0;
#endif

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
void log_emit(LogSeverity sev, int kind, const char *msg) {
    for (int i = 0; i < g_active_count; ++i) {
        LogSink *s = g_active[i];
        if (sev < s->min_severity)
            continue;
        s->write(s, sev, kind, msg);
    }
}

/* --- File sink: plain text, one record per line ------------------------- */

void file_sink_write(LogSink *s, LogSeverity sev, int kind, const char *msg) {
    if (!s->fp)
        return;
    if (kind == REC_SECTION_BEGIN)
        fprintf(s->fp, "==== %s ====\n", msg);
    else if (kind == REC_SECTION_END)
        ; /* no closer in the file */
    else if (kind == REC_RAW || kind == REC_BANNER)
        fprintf(s->fp, "%s\n", msg); /* banner is colour-only; file stays plain */
    else
        fprintf(s->fp, "[%s] %s\n", severity_tag(sev), msg);
    fflush(s->fp);
}

/* --- Terminal sink: ANSI 16-colour on stderr, TTY-gated ----------------- */

const char *severity_color(LogSeverity sev) {
    switch (sev) {
    case LOG_DEBUG:
        return "\033[90m"; /* dim grey */
    case LOG_WARN:
        return "\033[33m"; /* yellow */
    case LOG_ERROR:
        return "\033[31m"; /* red */
    case LOG_INFO:
        return ""; /* default */
    }
    return "";
}

void term_sink_write(LogSink *s, LogSeverity sev, int kind, const char *msg) {
    FILE *out = s->fp; /* stderr */
    if (kind == REC_SECTION_BEGIN) {
        fprintf(out, "\033[36m------ %s ", msg);
        int used = 7 + (int)strlen(msg) + 1; /* "------ " + msg + " " */
        for (int i = used; i < LOG_TERM_WIDTH; ++i)
            fputc('-', out);
        fputs("\033[0m\n", out);
    } else if (kind == REC_SECTION_END) {
        fputs("\033[36m", out);
        for (int i = 0; i < LOG_TERM_WIDTH; ++i)
            fputc('-', out);
        fputs("\033[0m\n", out);
    } else if (kind == REC_BANNER) {
        fprintf(out, "\033[36m%s\033[0m\n", msg); /* cyan, like section rules */
    } else if (kind == REC_RAW) {
        fprintf(out, "%s\n", msg);
    } else {
        fprintf(out, "%s%s\033[0m\n", severity_color(sev), msg);
    }
    fflush(out);
}

/* Registered but inert: the terminal sink's no-op when stderr is not a TTY (or
 * NO_COLOR is set), so redirected output never gets ANSI escapes. */
void noop_write(LogSink *, LogSeverity, int, const char *) {}

/* --- Console buffer sink: text into the in-engine scrollback ---------------
 * Severity is conveyed to the engine adapter (it colours the line), so unlike
 * the file sink there is no "[INFO]"/"[WARN]" text tag — the message goes
 * through verbatim and the colour carries the level. */

void console_sink_write(LogSink *s, LogSeverity sev, int kind, const char *msg) {
    if (!s->line_fn)
        return;
#ifndef LBA_LOG_NO_SDL
    /* The console scrollback ring is not thread-safe — only the main thread may
     * push. Off-thread records (e.g. the audio callback) are dropped here; they
     * still reach the file and terminal sinks. */
    if (g_main_thread != 0 && SDL_GetCurrentThreadID() != g_main_thread)
        return;
#endif
    if (!msg)
        msg = "";
    if (kind == REC_SECTION_END)
        return; /* no closer line in the console */
    char line[LOG_MSG_MAX];
    LogLineKind lkind = LOG_LINE_NORMAL;
    if (kind == REC_SECTION_BEGIN) {
        (void)snprintf(line, sizeof line, "== %s ==", msg);
        lkind = LOG_LINE_SECTION;
    } else { /* REC_NORMAL / REC_RAW / REC_BANNER: verbatim, colour carries level */
        (void)snprintf(line, sizeof line, "%s", msg);
        if (kind == REC_BANNER)
            lkind = LOG_LINE_BANNER;
    }
    s->line_fn(line, sev, lkind);
}

int terminal_enabled(void) {
#if defined(_WIN32)
    if (!_isatty(_fileno(stderr)))
        return 0;
#else
    if (!isatty(fileno(stderr)))
        return 0;
#endif
    const char *nc = getenv("NO_COLOR");
    if (nc && nc[0] != '\0')
        return 0;
#if defined(_WIN32)
    /* Turn on ANSI escape processing for the console; bail if unavailable so we
     * don't spew raw escapes. */
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (h == INVALID_HANDLE_VALUE || !GetConsoleMode(h, &mode))
        return 0;
    if (!SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return 0;
#endif
    return 1;
}

/* --- Sink pool ---------------------------------------------------------- */

LogSink *alloc_sink(void) {
    for (int i = 0; i < LOG_MAX_SINKS; ++i) {
        if (!g_pool[i].in_use) {
            LogSink *s = &g_pool[i];
            s->in_use = 1;
            s->min_severity = LOG_DEBUG;
            s->write = 0;
            s->fp = 0;
            s->owns_fp = 0;
            s->line_fn = 0;
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

int sdl_category_to_kind(int category) {
    if (category == LBA_CAT_SECTION)
        return REC_SECTION_BEGIN;
    if (category == LBA_CAT_SECTION_END)
        return REC_SECTION_END;
    if (category == LBA_CAT_RAW)
        return REC_RAW;
    if (category == LBA_CAT_BANNER)
        return REC_BANNER;
    return REC_NORMAL;
}

int kind_to_sdl_category(int kind) {
    if (kind == REC_SECTION_BEGIN)
        return LBA_CAT_SECTION;
    if (kind == REC_SECTION_END)
        return LBA_CAT_SECTION_END;
    if (kind == REC_RAW)
        return LBA_CAT_RAW;
    if (kind == REC_BANNER)
        return LBA_CAT_BANNER;
    return LBA_CAT_LOG;
}

/* The single fan-out point. Installing this also routes the engine's existing
 * SDL_Log sites and SDL's own messages through our sinks (see the plan). */
void SDLCALL sdl_output(void *userdata, int category, SDL_LogPriority pri,
                        const char *message) {
    (void)userdata;
    log_emit(sdl_to_sev(pri), sdl_category_to_kind(category), message);
}
#endif /* !LBA_LOG_NO_SDL */

/* Direct fallback used when no sink is registered yet — before Log_Init /
 * Log_AddSink (e.g. the game-data-directory checks in main, which run before
 * InitAdeline) or after Log_Shutdown. Writes to stderr in the file sink's idiom
 * and bypasses the SDL spine (which may not even be installed pre-Log_Init), so
 * an early Log_Error/Log_Warn is still seen. Mirrors LogPrintf's fallback. */
void log_fallback_write(LogSeverity sev, int kind, const char *msg) {
    if (!msg)
        msg = "";
    if (kind == REC_SECTION_END)
        return;
    if (kind == REC_SECTION_BEGIN)
        fprintf(stderr, "==== %s ====\n", msg);
    else if (kind == REC_RAW || kind == REC_BANNER)
        fprintf(stderr, "%s\n", msg);
    else
        fprintf(stderr, "[%s] %s\n", severity_tag(sev), msg);
    fflush(stderr);
}

/* Transport seam: format-then-dispatch. Under SDL the record rides the log
 * spine and comes back through sdl_output; in host tests it goes straight to
 * the fan-out, so the core needs no SDL_Init. With no sinks registered, fall
 * back to a direct write so nothing is lost before the sinks come up. */
void emit_formatted(LogSeverity sev, int kind, const char *msg) {
    if (g_active_count == 0) {
        log_fallback_write(sev, kind, msg);
        return;
    }
#ifdef LBA_LOG_NO_SDL
    log_emit(sev, kind, msg);
#else
    SDL_LogMessage(kind_to_sdl_category(kind), sev_to_sdl(sev), "%s", msg);
#endif
}

void emit_va(LogSeverity sev, const char *fmt, va_list ap) {
    char buf[LOG_MSG_MAX];
    (void)vsnprintf(buf, sizeof buf, fmt, ap);
    emit_formatted(sev, REC_NORMAL, buf);
}

} // namespace

extern "C" {

void Log_Init(void) {
    g_active_count = 0;
    g_console_sink = 0;
    for (int i = 0; i < LOG_MAX_SINKS; ++i)
        g_pool[i].in_use = 0;
#ifndef LBA_LOG_NO_SDL
    g_main_thread = SDL_GetCurrentThreadID();
    /* SDL applies a per-category priority filter *before* calling the output
       function, and custom categories default to a high threshold — so without
       this our INFO/DEBUG records would be dropped before reaching any sink.
       Open our categories down to DEBUG and let the per-sink min-severity do the
       real filtering. */
    SDL_SetLogPriority(LBA_CAT_LOG, SDL_LOG_PRIORITY_DEBUG);
    SDL_SetLogPriority(LBA_CAT_SECTION, SDL_LOG_PRIORITY_DEBUG);
    SDL_SetLogPriority(LBA_CAT_SECTION_END, SDL_LOG_PRIORITY_DEBUG);
    SDL_SetLogPriority(LBA_CAT_RAW, SDL_LOG_PRIORITY_DEBUG);
    SDL_SetLogPriority(LBA_CAT_BANNER, SDL_LOG_PRIORITY_DEBUG);
    SDL_SetLogOutputFunction(sdl_output, 0);
#endif
}

void Log_Shutdown(void) {
#ifndef LBA_LOG_NO_SDL
    SDL_SetLogOutputFunction(0, 0);
#endif
    for (int i = 0; i < LOG_MAX_SINKS; ++i) {
        if (g_pool[i].in_use && g_pool[i].owns_fp && g_pool[i].fp) {
            fclose(g_pool[i].fp);
            g_pool[i].fp = 0;
        }
        g_pool[i].in_use = 0;
    }
    g_active_count = 0;
    g_console_sink = 0;
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

void Log_Raw(const char *fmt, ...) {
    char buf[LOG_MSG_MAX];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    emit_formatted(LOG_INFO, REC_RAW, buf);
}

void Log_Banner(const char *fmt, ...) {
    char buf[LOG_MSG_MAX];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    emit_formatted(LOG_INFO, REC_BANNER, buf);
}

void Log_BeginSection(const char *title) {
    emit_formatted(LOG_INFO, REC_SECTION_BEGIN, title ? title : "");
}

void Log_EndSection(void) {
    emit_formatted(LOG_INFO, REC_SECTION_END, "");
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
    s->owns_fp = 1;
    s->min_severity = min_severity;
    s->write = file_sink_write;
    return s;
}

LogSink *Log_MakeTerminalSink(LogSeverity min_severity) {
    LogSink *s = alloc_sink();
    if (!s)
        return 0;
    s->fp = stderr; /* not owned — never closed on shutdown */
    s->min_severity = min_severity;
    s->write = terminal_enabled() ? term_sink_write : noop_write;
    return s;
}

LogSink *Log_MakeConsoleBufferSink(LogLineFn printer) {
    LogSink *s = alloc_sink();
    if (!s)
        return 0;
    /* INFO by default: shows the boot log + warnings, hides DEBUG chatter. The
     * verbose audio/SFX logs are gated off (sfxLogEnabled/musicLogEnabled), so
     * INFO stays clean without filtering the boot log out of the ring. */
    s->min_severity = LOG_INFO;
    s->write = console_sink_write;
    s->line_fn = printer;
    g_console_sink = s;
    return s;
}

void Log_SetConsoleSeverity(LogSeverity min_severity) {
    if (g_console_sink)
        g_console_sink->min_severity = min_severity;
}

LogSeverity Log_GetConsoleSeverity(void) {
    return g_console_sink ? g_console_sink->min_severity : LOG_INFO;
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

int Log_HasSinks(void) { return g_active_count > 0; }

} // extern "C"
