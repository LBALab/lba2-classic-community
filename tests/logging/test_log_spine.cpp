/* Host test for the REAL SDL_Log spine (docs/BOOT_LOG_PLAN.md).
 *
 * Unlike test_log (which compiles with -DLBA_LOG_NO_SDL and dispatches straight
 * to the sinks), this target builds LOG.CPP *with* the SDL spine and links SDL3.
 * It exercises the production path that test_log can't reach:
 *
 *   Log_* -> SDL_LogMessage -> the installed output function -> sdl_output ->
 *   log_emit -> sink
 *
 * including the category<->kind and severity<->priority mapping and, crucially,
 * the SDL_SetLogPriority(LBA_CAT_*, DEBUG) opens in Log_Init -- without those,
 * SDL's per-category filter drops INFO/DEBUG before any sink sees them (the
 * gotcha that bit the boot-log work). A break in the trampoline fails here. */
#include <SYSTEM/LOG.H>
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

/* Every severity + raw + section round-trips through the SDL spine to the file
 * sink with its tag/idiom intact. (If the SetLogPriority opens regressed, the
 * DEBUG/INFO assertions fail — SDL would have filtered them out.) */
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
    ASSERT_TRUE(strstr(buf, "banner-line") != NULL);        /* LBA_CAT_BANNER round-trip */
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

int main(void) {
    RUN_TEST(test_spine_roundtrip);
    RUN_TEST(test_spine_routes_bare_sdl_log);
    RUN_TEST(test_spine_min_severity);
    TEST_SUMMARY();
    return test_failures != 0;
}
