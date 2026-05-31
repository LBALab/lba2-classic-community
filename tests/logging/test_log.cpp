/* Host test for the Phase 1 log core + file sink (docs/BOOT_LOG_PLAN.md).
 *
 * Built with -DLBA_LOG_NO_SDL, so the log core's format-then-dispatch goes
 * straight to the fan-out with no SDL_Init — the file sink is exercised
 * end-to-end and its on-disk content asserted. */
#include "LOG.H"
#include "test_harness.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TMP "test_log_phase1.tmp"

/* Recording stub for the console buffer sink (LOG.CPP -> Console_Print). The
 * real console module isn't linked into host tests; capture the pushed lines so
 * the sink's rendering and filtering can be asserted. */
static char g_console_capture[4096];
extern "C" void Console_Print(const char *fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
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
    LogSink *s = Log_MakeConsoleBufferSink();
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

int main(void) {
    RUN_TEST(test_severity_prefixes);
    RUN_TEST(test_min_severity_filter);
    RUN_TEST(test_section_header);
    RUN_TEST(test_scoped_section);
    RUN_TEST(test_console_sink);
    TEST_SUMMARY();
    return test_failures != 0;
}
