/* Host test for the Phase 1 log core + file sink (docs/BOOT_LOG_PLAN.md).
 *
 * Built with -DLBA_LOG_NO_SDL, so the log core's format-then-dispatch goes
 * straight to the fan-out with no SDL_Init — the file sink is exercised
 * end-to-end and its on-disk content asserted. */
#include "LOG.H"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

#define TMP "test_log_phase1.tmp"

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

int main(void) {
    RUN_TEST(test_severity_prefixes);
    RUN_TEST(test_min_severity_filter);
    RUN_TEST(test_section_header);
    RUN_TEST(test_scoped_section);
    TEST_SUMMARY();
    return test_failures != 0;
}
