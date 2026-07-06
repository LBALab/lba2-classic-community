/* Host test for the Phase 1 log core + file sink (docs/BOOT_LOG_PLAN.md).
 *
 * Built with -DLBA_LOG_NO_SDL, so the log core's format-then-dispatch goes
 * straight to the fan-out with no SDL_Init — the file sink is exercised
 * end-to-end and its on-disk content asserted. */
#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H>
#include "test_harness.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h> /* dup/dup2 — capture stderr for the no-sink fallback test */

#define TMP "test_log_phase1.tmp"
#define TMP2 "test_log_phase1b.tmp"

/* The console buffer sink calls this per line (injected via
 * Log_MakeConsoleBufferSink) — capture the lines so the sink's rendering and
 * filtering can be asserted without linking the real console module. */
static char g_console_capture[4096];
/* Per-call (line, severity, kind) record so tests can assert what the console
 * needs to colour the line — the console renders level/structure as colour, so
 * the sink passes severity + kind instead of a "[INFO]" text tag. */
struct ConsoleCall {
    char line[256];
    LogSeverity sev;
    LogLineKind kind;
};
static struct ConsoleCall g_console_calls[128];
static int g_console_call_count;
static void record_console_line(const char *line, LogSeverity sev, LogLineKind kind) {
    if (g_console_call_count < 128) {
        snprintf(g_console_calls[g_console_call_count].line, 256, "%s", line);
        g_console_calls[g_console_call_count].sev = sev;
        g_console_calls[g_console_call_count].kind = kind;
        g_console_call_count++;
    }
    size_t used = strlen(g_console_capture);
    snprintf(g_console_capture + used, sizeof g_console_capture - used, "%s\n", line);
}
static void console_capture_reset(void) {
    g_console_capture[0] = '\0';
    g_console_call_count = 0;
}
static const struct ConsoleCall *console_call_of(const char *needle) {
    for (int i = 0; i < g_console_call_count; i++)
        if (strstr(g_console_calls[i].line, needle))
            return &g_console_calls[i];
    return 0;
}
/* Severity recorded for the first captured line containing needle, or -1. */
static int console_sev_of(const char *needle) {
    const struct ConsoleCall *c = console_call_of(needle);
    return c ? (int)c->sev : -1;
}

static void slurp(const char *path, char *out, int max) {
    FILE *f = fopen(path, "r");
    if (!f) {
        out[0] = '\0';
        return;
    }
    size_t n = fread(out, 1, (size_t)(max - 1), f);
    out[n] = '\0';
    fclose(f);
}

/* Each severity writes its tagged line. */
static void test_severity_prefixes(void) {
    remove(TMP);
    Log_Init();
    LogSink *s = Log_MakeFileSink(TMP, LOG_DEBUG);
    ASSERT_TRUE(s != NULL);
    Log_AddSink(s);
    Log_Debug("d%d", 1);
    Log_Info("eye");
    Log_Warn("warn!");
    Log_Error("err");
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "[DEBUG] d1") != NULL);
    ASSERT_TRUE(strstr(buf, "[INFO] eye") != NULL);
    ASSERT_TRUE(strstr(buf, "[WARN] warn!") != NULL);
    ASSERT_TRUE(strstr(buf, "[ERROR] err") != NULL);
    remove(TMP);
}

/* A WARN+ sink drops DEBUG/INFO. */
static void test_min_severity_filter(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_WARN));
    Log_Debug("nope-d");
    Log_Info("nope-i");
    Log_Warn("yes-w");
    Log_Error("yes-e");
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "nope-d") == NULL);
    ASSERT_TRUE(strstr(buf, "nope-i") == NULL);
    ASSERT_TRUE(strstr(buf, "[WARN] yes-w") != NULL);
    ASSERT_TRUE(strstr(buf, "[ERROR] yes-e") != NULL);
    remove(TMP);
}

/* Sections render as "==== Title ====". */
static void test_section_header(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    Log_BeginSection("Boot");
    Log_Info("inside");
    Log_EndSection();
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "==== Boot ====") != NULL);
    ASSERT_TRUE(strstr(buf, "[INFO] inside") != NULL);
    remove(TMP);
}

/* Log_Banner is verbatim to the file sink — the cyan tint is the terminal
 * sink's doing alone, so the on-disk text is identical to a raw line. */
static void test_banner_verbatim_to_file(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    Log_Banner("======== Ready in %.2fs ========", 0.73);
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strcmp(buf, "======== Ready in 0.73s ========\n") == 0);
    remove(TMP);
}

/* The C++ ScopedSection RAII helper opens a section on construction. */
static void test_scoped_section(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    {
        ScopedSection sec("Audio");
        Log_Info("device up");
    }
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "==== Audio ====") != NULL);
    ASSERT_TRUE(strstr(buf, "[INFO] device up") != NULL);
    remove(TMP);
}

/* Console buffer sink: text into the scrollback with NO severity text tag (the
 * console renders the level as colour), raw/banner verbatim, section idiom. The
 * severity/kind still ride along to the printer, asserted via console_sev_of.
 * Gating is by the global level (Log_SetLevel), shared with the file/terminal
 * sinks, not a per-console knob. (Thread gating is SDL-only and compiled out
 * under LBA_LOG_NO_SDL, so the sink always writes here.) */
static void test_console_sink(void) {
    console_capture_reset();
    Log_Init();
    LogSink *s = Log_MakeConsoleBufferSink(record_console_line);
    ASSERT_TRUE(s != NULL);
    Log_AddSink(s);

    /* Engine default is INFO: DEBUG dropped; INFO/WARN/raw/banner all verbatim
     * (no tag); section idiom; severity conveyed out-of-band for the colour. */
    Log_SetLevel(LOG_INFO);
    Log_Debug("dbg-line");
    Log_Info("info-line");
    Log_Warn("warn-line");
    Log_Raw("raw-line");
    Log_Banner("banner-line");
    Log_BeginSection("Boot");
    ASSERT_TRUE(strstr(g_console_capture, "dbg-line") == NULL);
    ASSERT_TRUE(strstr(g_console_capture, "info-line") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "warn-line") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "raw-line") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "banner-line") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "== Boot ==") != NULL);
    /* No severity text tag reaches the console — colour replaces it. */
    ASSERT_TRUE(strstr(g_console_capture, "[INFO]") == NULL);
    ASSERT_TRUE(strstr(g_console_capture, "[WARN]") == NULL);
    /* …but the severity is handed to the printer so it can pick the colour. */
    ASSERT_TRUE(console_sev_of("info-line") == LOG_INFO);
    ASSERT_TRUE(console_sev_of("warn-line") == LOG_WARN);
    /* Kind is handed across too, so the adapter can tint banners/sections cyan. */
    ASSERT_TRUE(console_call_of("info-line")->kind == LOG_LINE_NORMAL);
    ASSERT_TRUE(console_call_of("banner-line")->kind == LOG_LINE_BANNER);
    ASSERT_TRUE(console_call_of("raw-line")->kind == LOG_LINE_NORMAL);
    ASSERT_TRUE(console_call_of("== Boot ==")->kind == LOG_LINE_SECTION);

    /* Raising the global level to WARN drops INFO from the console too. */
    console_capture_reset();
    Log_SetLevel(LOG_WARN);
    Log_Info("info-after");
    Log_Error("err-after");
    ASSERT_TRUE(strstr(g_console_capture, "info-after") == NULL);
    ASSERT_TRUE(strstr(g_console_capture, "err-after") != NULL);
    ASSERT_TRUE(console_sev_of("err-after") == LOG_ERROR);

    Log_Shutdown();
}

/* The global level (Log_SetLevel) is the master gate: it drops records for every
 * sink before the sink's own min_severity is consulted, and it does so for the
 * file sink too, even one opened at LOG_DEBUG. Lowering the level reveals them. */
static void test_global_floor(void) {
    remove(TMP);
    console_capture_reset();
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG)); /* sink itself admits DEBUG+ */
    Log_AddSink(Log_MakeConsoleBufferSink(record_console_line));

    /* Level at INFO: DEBUG is dropped for BOTH sinks despite the file sink's own
     * DEBUG min: the global level wins over the per-sink setting. */
    Log_SetLevel(LOG_INFO);
    Log_Debug("floor-hidden");
    Log_Info("floor-info");
    /* Level lowered to DEBUG: a DEBUG record now reaches both sinks. */
    Log_SetLevel(LOG_DEBUG);
    Log_Debug("floor-shown");
    ASSERT_TRUE(Log_GetLevel() == LOG_DEBUG); /* getter round-trips */
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "floor-hidden") == NULL);        /* gated by the level */
    ASSERT_TRUE(strstr(buf, "[INFO] floor-info") != NULL);   /* INFO passes at INFO */
    ASSERT_TRUE(strstr(buf, "[DEBUG] floor-shown") != NULL); /* passes once lowered */
    ASSERT_TRUE(strstr(g_console_capture, "floor-hidden") == NULL);
    ASSERT_TRUE(strstr(g_console_capture, "floor-info") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "floor-shown") != NULL);
    remove(TMP);
}

/* One Log_* call fans out to every registered sink at once, and each sink's own
 * min-severity still applies independently, shown here by a second, WARN-only
 * file sink that drops a DEBUG record the DEBUG file sink keeps. */
static void test_fanout_to_all_sinks(void) {
    remove(TMP);
    remove(TMP2);
    console_capture_reset();
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG)); /* admits DEBUG+ */
    Log_AddSink(Log_MakeFileSink(TMP2, LOG_WARN)); /* admits WARN+ */
    Log_AddSink(Log_MakeConsoleBufferSink(record_console_line));

    Log_Warn("fan-%d", 7); /* one call -> all three (all admit WARN) */
    Log_Debug("dbg-only"); /* one call -> DEBUG file + console; WARN file drops it */
    Log_Shutdown();

    char buf[4096], buf2[4096];
    slurp(TMP, buf, sizeof buf);
    slurp(TMP2, buf2, sizeof buf2);
    /* WARN reached every sink from a single emit. The files keep the text tag;
     * the console drops it (colour carries the level) but gets the message. */
    ASSERT_TRUE(strstr(buf, "[WARN] fan-7") != NULL);
    ASSERT_TRUE(strstr(buf2, "[WARN] fan-7") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "fan-7") != NULL);
    ASSERT_TRUE(console_sev_of("fan-7") == LOG_WARN);
    /* Per-sink min still filters independently inside the fan-out: DEBUG reaches
     * the DEBUG file and the console (which admits all), but not the WARN file. */
    ASSERT_TRUE(strstr(buf, "[DEBUG] dbg-only") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "dbg-only") != NULL);
    ASSERT_TRUE(strstr(buf2, "dbg-only") == NULL);
    remove(TMP);
    remove(TMP2);
}

/* The sink pool is fixed-size: past capacity Log_Make*Sink returns NULL (drops
 * silently, never aborts), and logging with a full pool stays safe. */
static void test_sink_pool_limit(void) {
    Log_Init();
    LogSink *s = NULL;
    int made = 0;
    for (int i = 0; i < 64; ++i) { /* well past LOG_MAX_SINKS */
        s = Log_MakeFileSink("pool_probe.tmp", LOG_DEBUG);
        if (s)
            ++made;
    }
    ASSERT_TRUE(made >= 1);  /* some allocations succeeded */
    ASSERT_TRUE(s == NULL);  /* and the pool eventually said no, gracefully */
    Log_Info("still alive"); /* emitting with a full pool must not crash */
    Log_Shutdown();
    remove("pool_probe.tmp");
}

/* Log_RemoveSink takes a sink off the dispatch list; others keep receiving. */
static void test_remove_sink(void) {
    remove(TMP);
    console_capture_reset();
    Log_Init();
    LogSink *fileSink = Log_MakeFileSink(TMP, LOG_DEBUG);
    Log_AddSink(fileSink);
    Log_AddSink(Log_MakeConsoleBufferSink(record_console_line));

    Log_Info("before-remove");
    Log_RemoveSink(fileSink);
    Log_Info("after-remove");
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "before-remove") != NULL);
    /* after RemoveSink the file sink stops; the console sink keeps receiving */
    ASSERT_TRUE(strstr(buf, "after-remove") == NULL);
    ASSERT_TRUE(strstr(g_console_capture, "after-remove") != NULL);
    remove(TMP);
}

/* --- Legacy LogPrintf/LogPuts shim (U2): routes through the fan-out --------- */

/* LogPrintf reaches the sinks verbatim — no [INFO] tag, exactly one trailing
 * newline (the sink adds it), no doubling. */
static void test_logprintf_routes_through_fanout(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    LogPrintf("hello %d\n", 42);
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strcmp(buf, "hello 42\n") == 0);
    remove(TMP);
}

/* A multi-line message becomes one record per line: no doubled newlines, no
 * spurious trailing blank line. */
static void test_logprintf_multiline_split(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    LogPrintf("a\nb\n");
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strcmp(buf, "a\nb\n") == 0);
    remove(TMP);
}

/* LogPuts passes its string as an argument, so '%' is never a format. */
static void test_logputs_percent_safe(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    LogPuts("100% complete");
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strcmp(buf, "100% complete\n") == 0);
    remove(TMP);
}

/* A message with no trailing newline becomes one terminated line (the fan-out
 * is line-oriented). Documents the one behavior change from byte-concatenation. */
static void test_logprintf_partial_line_terminated(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    LogPrintf("partial");
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strcmp(buf, "partial\n") == 0);
    remove(TMP);
}

/* Before any sink exists the shim must not lose the line: it falls back to a
 * direct write to the log file (and stderr). */
static void test_logprintf_early_boot_fallback(void) {
    remove("shim_fallback.tmp");
    Log_Init();                     /* resets the registry; no sink added */
    CreateLog("shim_fallback.tmp"); /* sets the log path, truncates it */
    LogPrintf("early-boot\n");      /* no sinks -> direct fallback to the file */
    Log_Shutdown();

    char buf[4096];
    slurp("shim_fallback.tmp", buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "early-boot") != NULL);
    remove("shim_fallback.tmp");
}

/* Terminal sink with stderr redirected (not a TTY): it still EMITS plain,
 * severity-tagged lines with no ANSI, so a harness / piped / CI run sees the log
 * inline on stderr instead of only in adeline.log. The sink's own min_severity
 * still applies. Capture stderr to assert. */
static void test_terminal_sink_plain(void) {
    Log_Init();

    fflush(stderr);
    int saved = dup(fileno(stderr));
    FILE *cap = fopen("term_stderr.tmp", "w");
    ASSERT_TRUE(cap != NULL);
    dup2(fileno(cap), fileno(stderr));

    /* Built now, with stderr pointing at the file -> not a TTY -> plain mode. */
    Log_AddSink(Log_MakeTerminalSink(LOG_INFO));
    Log_Info("term-info");
    Log_Warn("term-warn");
    Log_BeginSection("Boot");
    Log_Debug("term-debug-drop"); /* below the sink's INFO min -> dropped */

    fflush(stderr);
    dup2(saved, fileno(stderr)); /* restore */
    close(saved);
    fclose(cap);

    char buf[2048];
    slurp("term_stderr.tmp", buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "[INFO] term-info") != NULL);
    ASSERT_TRUE(strstr(buf, "[WARN] term-warn") != NULL);
    ASSERT_TRUE(strstr(buf, "==== Boot ====") != NULL);  /* plain section idiom */
    ASSERT_TRUE(strstr(buf, "term-debug-drop") == NULL); /* INFO sink drops DEBUG */
    ASSERT_TRUE(strstr(buf, "\033[") == NULL);           /* no ANSI escapes piped */
    remove("term_stderr.tmp");
    Log_Shutdown();
}

/* With no sink registered, Log_* must still surface — it falls back to a direct
 * stderr write (bypassing the SDL spine). This is what keeps the pre-Log_Init
 * game-data-directory errors visible. Capture stderr to assert it. */
static void test_log_fallback_no_sinks(void) {
    Log_Init(); /* no sink added -> g_active_count == 0 */

    fflush(stderr);
    int saved = dup(fileno(stderr));
    FILE *cap = fopen("fallback_stderr.tmp", "w");
    ASSERT_TRUE(cap != NULL);
    dup2(fileno(cap), fileno(stderr));

    Log_Error("orphan-%d", 9); /* no sinks -> direct fallback to stderr */

    fflush(stderr);
    dup2(saved, fileno(stderr)); /* restore */
    close(saved);
    fclose(cap);

    char buf[1024];
    slurp("fallback_stderr.tmp", buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "[ERROR] orphan-9") != NULL);
    remove("fallback_stderr.tmp");
    Log_Shutdown();
}

int main(void) {
    RUN_TEST(test_severity_prefixes);
    RUN_TEST(test_min_severity_filter);
    RUN_TEST(test_section_header);
    RUN_TEST(test_banner_verbatim_to_file);
    RUN_TEST(test_scoped_section);
    RUN_TEST(test_console_sink);
    RUN_TEST(test_global_floor);
    RUN_TEST(test_fanout_to_all_sinks);
    RUN_TEST(test_sink_pool_limit);
    RUN_TEST(test_remove_sink);
    RUN_TEST(test_logprintf_routes_through_fanout);
    RUN_TEST(test_logprintf_multiline_split);
    RUN_TEST(test_logputs_percent_safe);
    RUN_TEST(test_logprintf_partial_line_terminated);
    RUN_TEST(test_terminal_sink_plain);
    RUN_TEST(test_logprintf_early_boot_fallback);
    RUN_TEST(test_log_fallback_no_sinks);
    TEST_SUMMARY();
    return test_failures != 0;
}
