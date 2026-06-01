/* Host test for the Phase 1 log core + file sink (docs/BOOT_LOG_PLAN.md).
 *
 * Built with -DLBA_LOG_NO_SDL, so the log core's format-then-dispatch goes
 * straight to the fan-out with no SDL_Init — the file sink is exercised
 * end-to-end and its on-disk content asserted. */
#include <SYSTEM/LOG.H>
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

#define TMP "test_log_phase1.tmp"

/* The console buffer sink calls this per line (injected via
 * Log_MakeConsoleBufferSink) — capture the lines so the sink's rendering and
 * filtering can be asserted without linking the real console module. */
static char g_console_capture[4096];
static void record_console_line(const char *line) {
    size_t used = strlen(g_console_capture);
    snprintf(g_console_capture + used, sizeof g_console_capture - used, "%s\n", line);
}
static void console_capture_reset(void) { g_console_capture[0] = '\0'; }

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

/* Console buffer sink: monochrome text into the scrollback, INFO default,
 * raw verbatim, runtime severity change. (Thread gating is SDL-only and
 * compiled out under LBA_LOG_NO_SDL, so the sink always writes here.) */
static void test_console_sink(void) {
    console_capture_reset();
    Log_Init();
    LogSink *s = Log_MakeConsoleBufferSink(record_console_line);
    ASSERT_TRUE(s != NULL);
    Log_AddSink(s);
    ASSERT_TRUE(Log_GetConsoleSeverity() == LOG_INFO);

    /* Default INFO: DEBUG dropped, INFO/WARN tagged, raw verbatim, section idiom. */
    Log_Debug("dbg-line");
    Log_Info("info-line");
    Log_Warn("warn-line");
    Log_Raw("raw-line");
    Log_BeginSection("Boot");
    ASSERT_TRUE(strstr(g_console_capture, "dbg-line") == NULL);
    ASSERT_TRUE(strstr(g_console_capture, "[INFO] info-line") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "[WARN] warn-line") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "raw-line") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "[INFO] raw-line") == NULL); /* verbatim */
    ASSERT_TRUE(strstr(g_console_capture, "== Boot ==") != NULL);

    /* loglevel raise to WARN now drops INFO. */
    console_capture_reset();
    Log_SetConsoleSeverity(LOG_WARN);
    ASSERT_TRUE(Log_GetConsoleSeverity() == LOG_WARN);
    Log_Info("info-after");
    Log_Error("err-after");
    ASSERT_TRUE(strstr(g_console_capture, "info-after") == NULL);
    ASSERT_TRUE(strstr(g_console_capture, "[ERROR] err-after") != NULL);

    Log_Shutdown();
}

/* One Log_* call fans out to every registered sink at once, and each sink's own
 * min-severity still applies independently. */
static void test_fanout_to_all_sinks(void) {
    remove(TMP);
    console_capture_reset();
    Log_Init();
    /* file sink admits DEBUG+, console sink admits INFO+ */
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    Log_AddSink(Log_MakeConsoleBufferSink(record_console_line));

    Log_Warn("fan-%d", 7); /* one call -> both sinks (both admit WARN) */
    Log_Debug("dbg-only"); /* one call -> file only (console drops DEBUG) */
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    /* WARN reached both sinks from a single emit. */
    ASSERT_TRUE(strstr(buf, "[WARN] fan-7") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "[WARN] fan-7") != NULL);
    /* Per-sink filtering holds inside the fan-out: DEBUG to file, not console. */
    ASSERT_TRUE(strstr(buf, "[DEBUG] dbg-only") != NULL);
    ASSERT_TRUE(strstr(g_console_capture, "dbg-only") == NULL);
    remove(TMP);
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

int main(void) {
    RUN_TEST(test_severity_prefixes);
    RUN_TEST(test_min_severity_filter);
    RUN_TEST(test_section_header);
    RUN_TEST(test_scoped_section);
    RUN_TEST(test_console_sink);
    RUN_TEST(test_fanout_to_all_sinks);
    RUN_TEST(test_sink_pool_limit);
    RUN_TEST(test_remove_sink);
    TEST_SUMMARY();
    return test_failures != 0;
}
