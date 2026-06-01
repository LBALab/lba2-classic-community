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
 * Sinks: file (always), ANSI terminal (TTY-gated), and the in-engine console
 * buffer. The boot path wires all three in InitAdeline. */

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogSeverity;

typedef struct LogSink LogSink; /* opaque handle */

/* A formatted, ready-to-print line (no trailing newline). The console buffer
 * sink calls one of these for each record — the engine passes an adapter over
 * Console_Print, keeping this core free of any SOURCES dependency. */
typedef void (*LogLineFn)(const char *line);

/* Install the fan-out (no-op under LBA_LOG_NO_SDL) and reset the sink registry.
 * Shutdown flushes and closes sinks and uninstalls the fan-out. */
void Log_Init(void);
void Log_Shutdown(void);

/* Severity entry points — printf-style. Each formats once and fans out. */
void Log_Debug(const char *fmt, ...);
void Log_Info(const char *fmt, ...);
void Log_Warn(const char *fmt, ...);
void Log_Error(const char *fmt, ...);

/* Verbatim line — no severity prefix, no section idiom; sinks print it as-is.
 * For framing/banner output (e.g. the boot header). Admitted at INFO level for
 * per-sink filtering. */
void Log_Raw(const char *fmt, ...);

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

/* Console buffer sink: feeds each record to \a printer (the engine passes an
 * adapter over Console_Print), so the boot log and warnings show on the F12
 * overlay. Monochrome — the console ring stores plain text with no per-line
 * colour. Main-thread records only: the ring is not thread-safe, so off-thread
 * records (e.g. the audio callback) are dropped here but still reach the file
 * and terminal sinks. Defaults to INFO (the boot log is INFO/raw; the verbose
 * audio/SFX logs are gated off by default, so INFO stays clean); adjust live
 * with Log_SetConsoleSeverity / the `loglevel` console command. */
LogSink *Log_MakeConsoleBufferSink(LogLineFn printer);

/* Raise/lower the console buffer sink's minimum severity at runtime. No-op if
 * no console sink is registered. */
void Log_SetConsoleSeverity(LogSeverity min_severity);
LogSeverity Log_GetConsoleSeverity(void);

void Log_AddSink(LogSink *sink);
void Log_RemoveSink(LogSink *sink);

/* True once at least one sink is registered (the fan-out is live). Legacy
 * LogPrintf/LogPuts use this to choose between routing through the fan-out and
 * an early-boot direct write (before any sink exists). */
int Log_HasSinks(void);

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
