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
void CreateLog(const char *filePath) {
    FILE *logfile;

    strncpy(LogName, filePath, ADELINE_MAX_PATH);
    LogName[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee

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
