/* Production log transport: engine records dispatch directly; platform and
   SDL diagnostics enter through the selected backend. The console sink admits
   only the main thread, while file/terminal sinks also receive worker records. */
#include <SYSTEM/LOG.H>
#include <PLATFORM/PLATFORM.H>
#include "test_harness.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#define TMP "test_log_spine.tmp"

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

/* Record kinds and severities retain their file formatting. */
static void test_spine_roundtrip(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    Log_Debug("dbg");
    Log_Info("eye-%d", 7);
    Log_Warn("warn");
    Log_Error("err");
    Log_Raw("raw-line");
    Log_Banner("banner-line");
    Log_BeginSection("Sect");
    Log_EndSection();
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "[DEBUG] dbg") != NULL);
    ASSERT_TRUE(strstr(buf, "[INFO] eye-7") != NULL);
    ASSERT_TRUE(strstr(buf, "[WARN] warn") != NULL);
    ASSERT_TRUE(strstr(buf, "[ERROR] err") != NULL);
    ASSERT_TRUE(strstr(buf, "raw-line") != NULL);
    ASSERT_TRUE(strstr(buf, "banner-line") != NULL);
    ASSERT_TRUE(strstr(buf, "[INFO] banner-line") == NULL); /* verbatim, no tag */
    ASSERT_TRUE(strstr(buf, "==== Sect ====") != NULL);
    remove(TMP);
}

/* The engine's existing bare SDL_Log() sites (the audio stack, etc.) also reach
 * the fan-out once Log_Init installs the output function. */
static void test_spine_routes_bare_sdl_log(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    SDL_Log("bare-sdl-%d", 3); /* APPLICATION category, INFO priority */
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "bare-sdl-3") != NULL);
    remove(TMP);
}

/* Per-sink severity filtering still applies on the spine path. */
static void test_spine_min_severity(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_WARN));
    Log_Info("dropped-info");
    Log_Error("kept-error");
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "dropped-info") == NULL);
    ASSERT_TRUE(strstr(buf, "[ERROR] kept-error") != NULL);
    remove(TMP);
}

/* Structural framing bypasses the level; raw/ordinary records do not. */
static void test_spine_structural_bypasses_level(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    Log_SetLevel(LOG_ERROR);      /* strictest short of silence */
    Log_Banner("spine-banner");   /* structural -> survives */
    Log_BeginSection("SpineSec"); /* structural -> survives */
    Log_Raw("spine-raw");         /* REC_RAW -> gated by the level */
    Log_Info("spine-info");       /* REC_NORMAL below ERROR -> gated */
    Log_Error("spine-err");       /* REC_NORMAL at ERROR -> kept */
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "spine-banner") != NULL);
    ASSERT_TRUE(strstr(buf, "==== SpineSec ====") != NULL); /* clears SDL + gate */
    ASSERT_TRUE(strstr(buf, "spine-raw") == NULL);          /* Log_Raw obeys level */
    ASSERT_TRUE(strstr(buf, "spine-info") == NULL);         /* INFO gated at ERROR */
    ASSERT_TRUE(strstr(buf, "[ERROR] spine-err") != NULL);
    remove(TMP);
}

static int console_records = 0;
static void console_record(const char *, LogSeverity, LogLineKind) {
    console_records++;
}
static int SDLCALL emit_worker_records(void *) {
    Log_Info("worker-engine");
    Platform_Log(PLATFORM_LOG_WARN, "worker-platform");
    SDL_Log("worker-sdk");
    return 0;
}
static void test_platform_transport_and_main_thread_gate(void) {
    remove(TMP);
    console_records = 0;
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    Log_AddSink(Log_MakeConsoleBufferSink(console_record));
    Platform_Log(PLATFORM_LOG_INFO, "main-platform");
    ASSERT_EQ_INT(1, console_records);
    SDL_Thread *worker = SDL_CreateThread(emit_worker_records, "pal-log-fixture", NULL);
    ASSERT_TRUE(worker != NULL);
    if (worker)
        SDL_WaitThread(worker, NULL);
    ASSERT_EQ_INT(1, console_records);
    Log_Shutdown();
    char buf[4096];
    slurp(TMP, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "[INFO] main-platform") != NULL);
    ASSERT_TRUE(strstr(buf, "[INFO] worker-engine") != NULL);
    ASSERT_TRUE(strstr(buf, "[WARN] worker-platform") != NULL);
    ASSERT_TRUE(strstr(buf, "worker-sdk") != NULL);
    remove(TMP);
}

int main(void) {
    RUN_TEST(test_spine_roundtrip);
    RUN_TEST(test_spine_routes_bare_sdl_log);
    RUN_TEST(test_spine_min_severity);
    RUN_TEST(test_spine_structural_bypasses_level);
    RUN_TEST(test_platform_transport_and_main_thread_gate);
    TEST_SUMMARY();
    return test_failures != 0;
}
