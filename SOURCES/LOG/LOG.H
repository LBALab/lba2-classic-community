#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Structured logging: one call fans out to every registered sink, each
 * rendering the record its own way. Built on an SDL_Log spine (a single
 * SDL_SetLogOutputFunction owns the fan-out); host tests compile with
 * -DLBA_LOG_NO_SDL to bypass SDL entirely. See docs/BOOT_LOG_PLAN.md.
 *
 * Phase 1: core API + file sink. The console (Phase 2) and ANSI terminal
 * (Phase 3) sinks, and wiring into the boot path (Phase 4), come later. */

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogSeverity;

typedef struct LogSink LogSink; /* opaque handle */

/* Install the fan-out (no-op under LBA_LOG_NO_SDL) and reset the sink registry.
 * Shutdown flushes and closes sinks and uninstalls the fan-out. */
void Log_Init(void);
void Log_Shutdown(void);

/* Severity entry points — printf-style. Each formats once and fans out. */
void Log_Debug(const char *fmt, ...);
void Log_Info(const char *fmt, ...);
void Log_Warn(const char *fmt, ...);
void Log_Error(const char *fmt, ...);

/* Section markers — sinks render the header in their own idiom (the file sink
 * uses "==== Title ===="). EndSection has no file-sink output; it exists for
 * sinks that bracket (terminal/console, later phases) and for ScopedSection. */
void Log_BeginSection(const char *title);
void Log_EndSection(void);

/* File sink: appends to \a path, one record per line, flushed per write.
 * Records below \a min_severity are dropped. The engine passes adeline.log's
 * resolved path (GetLogPath) — one log file, reused, not a second one. */
LogSink *Log_MakeFileSink(const char *path, LogSeverity min_severity);

/* Terminal sink: ANSI 16-colour records on stderr (severity-coloured, cyan
 * "------ Title ------" section rules). Active only when stderr is a real TTY
 * and NO_COLOR is unset (Windows: also enables VT processing); otherwise it
 * registers but writes nothing, so redirected output never gets escape codes. */
LogSink *Log_MakeTerminalSink(LogSeverity min_severity);

void Log_AddSink(LogSink *sink);
void Log_RemoveSink(LogSink *sink);

#ifdef __cplusplus
}

/* C++-only RAII over Log_BeginSection / Log_EndSection. Best-effort: the boot
 * path exit()s on init failure without unwinding (see docs/BOOT_LOG_PLAN.md),
 * so don't rely on the destructor for log correctness — the file sink flushes
 * per line, so a section left unclosed by exit() still has its records on disk. */
class ScopedSection {
  public:
    explicit ScopedSection(const char *title) { Log_BeginSection(title); }
    ~ScopedSection() { Log_EndSection(); }

  private:
    ScopedSection(const ScopedSection &);
    ScopedSection &operator=(const ScopedSection &);
};
#endif

#endif /* LOG_H */
