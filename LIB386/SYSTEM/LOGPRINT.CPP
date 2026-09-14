//----------------------------------------------------------------------------
#include <SYSTEM/ADELINE.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/LIMITS.H>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------
static char LogName[ADELINE_MAX_PATH] = "";

//----------------------------------------------------------------------------
// The previous run's log sits beside this one with ".prev" before its extension,
// so a player who relaunches after a crash still has the run that crashed.
// Exactly one is kept.
void GetPreviousLogPath(char *outPath, U32 outSize, const char *filePath) {
    const char *base = filePath;
    const char *dot = NULL;
    const char *p;
    size_t stem;

    if (outSize == 0)
        return;

    for (p = filePath; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    for (p = base; *p; p++) {
        if (*p == '.')
            dot = p;
    }
    stem = dot ? (size_t)(dot - filePath) : strlen(filePath);

    if (stem + strlen(".prev") + strlen(filePath + stem) >= outSize) {
        outPath[0] = '\0';
        return;
    }
    memcpy(outPath, filePath, stem);
    strcpy(outPath + stem, ".prev");
    strcat(outPath, filePath + stem);
}

static void KeepPreviousLog(const char *filePath) {
    char prevPath[ADELINE_MAX_PATH];
    FILE *existing;

    GetPreviousLogPath(prevPath, sizeof(prevPath), filePath);
    if (!prevPath[0])
        return;

    // With no log to move, the older one stays.
    existing = fopen(filePath, "rb");
    if (!existing)
        return;
    fclose(existing);

    // rename onto an existing file fails on Windows.
    remove(prevPath);
    rename(filePath, prevPath);
}

//----------------------------------------------------------------------------
void CreateLog(const char *filePath) {
    FILE *logfile;

    strncpy(LogName, filePath, ADELINE_MAX_PATH);
    LogName[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee

    KeepPreviousLog(LogName);

    logfile = fopen(LogName, "wb");
    if (!logfile) {
        LogName[0] = 0;
    } else {
        fclose(logfile);
    }

    if (!logfile) {
        LogPrintf("Warning: Unable to create log file at '%s', "
                  "outputting only to console...\n",
                  LogName);
    }
}

//----------------------------------------------------------------------------
// Legacy LogPrintf/LogPuts now feed the structured log fan-out (SYSTEM/LOG) so
// their ~100 call sites share the file/terminal/console sinks, per-sink severity
// filtering, and TTY gating instead of writing straight to stdout + the file.
// Lines are emitted verbatim (REC_RAW, no severity tag) to preserve their
// original appearance. Before any sink exists (early boot: a pre-init failure,
// or CreateLog's own warning) there is nothing to fan out to, so we fall back to
// a direct write -- stderr plus the log file if one is open -- so nothing is lost.

// Emit one line (without its terminating newline) verbatim through the fan-out.
static void EmitLine(const char *p, size_t len) {
    char line[1024];
    if (len >= sizeof(line))
        len = sizeof(line) - 1;
    memcpy(line, p, len);
    line[len] = '\0';
    Log_Raw("%s", line); // line is the argument, not the format
}

// Treat '\n' as a line terminator: one record per line. Each sink appends its
// own newline, so this avoids doubled newlines; blank lines are preserved.
static void RouteOrFallback(const char *s) {
    if (Log_HasSinks()) {
        const char *p = s;
        for (;;) {
            const char *nl = strchr(p, '\n');
            if (nl) {
                EmitLine(p, (size_t)(nl - p));
                p = nl + 1;
            } else {
                if (*p)
                    EmitLine(p, strlen(p));
                break;
            }
        }
    } else {
        // No sinks yet -- preserve the message directly so it is not lost.
        fputs(s, stderr);
        if (LogName[0]) {
            FILE *logfile = fopen(LogName, "ab+");
            if (logfile) {
                fputs(s, logfile);
                fclose(logfile);
            }
        }
    }
}

//----------------------------------------------------------------------------
void LogPrintf(const char *format, ...) {
    char buf[1024];
    va_list arglist;

    va_start(arglist, format);
    (void)vsnprintf(buf, sizeof(buf), format, arglist);
    va_end(arglist);

    RouteOrFallback(buf);
}

//----------------------------------------------------------------------------
void LogPuts(const char *string) {
    LogPrintf("%s\n", string); // passes the string as an argument -> '%' is safe
}

//----------------------------------------------------------------------------
