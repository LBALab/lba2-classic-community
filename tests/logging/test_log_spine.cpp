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

/* The structural-framing exemption holds through the REAL SDL spine. With the
 * level raised to its strictest, the banner and section markers still clear BOTH
 * SDL's per-category filter (opened to DEBUG in Log_Init) AND log_emit's master
 * gate, while a same-INFO Log_Raw line -- which clears the SDL filter too -- is
 * dropped by the gate. This guards the end-to-end path test_log (NO_SDL) can't:
 * if the SetLogPriority opens or the category<->kind round-trip regressed, a
 * raised level would silently swallow the boot banner and this fails. */
static void test_spine_structural_bypasses_level(void) {
    remove(TMP);
    Log_Init();
    Log_AddSink(Log_MakeFileSink(TMP, LOG_DEBUG));
    Log_SetLevel(LOG_ERROR);      /* strictest short of silence */
    Log_Banner("spine-banner");   /* structural -> survives via LBA_CAT_BANNER */
    Log_BeginSection("SpineSec"); /* structural -> survives */
    Log_Raw("spine-raw");         /* REC_RAW -> gated by the level */
    Log_Info("spine-info");       /* REC_NORMAL below ERROR -> gated */
    Log_Error("spine-err");       /* REC_NORMAL at ERROR -> kept */
    Log_Shutdown();

    char buf[4096];
    slurp(TMP, buf, sizeof buf);
    ASSERT_TRUE(strstr(buf, "spine-banner") != NULL);       /* clears SDL + gate */
    ASSERT_TRUE(strstr(buf, "==== SpineSec ====") != NULL); /* clears SDL + gate */
    ASSERT_TRUE(strstr(buf, "spine-raw") == NULL);          /* Log_Raw obeys level */
    ASSERT_TRUE(strstr(buf, "spine-info") == NULL);         /* INFO gated at ERROR */
    ASSERT_TRUE(strstr(buf, "[ERROR] spine-err") != NULL);
    remove(TMP);
}

int main(void) {
    RUN_TEST(test_spine_roundtrip);
    RUN_TEST(test_spine_routes_bare_sdl_log);
    RUN_TEST(test_spine_min_severity);
    RUN_TEST(test_spine_structural_bypasses_level);
    TEST_SUMMARY();
    return test_failures != 0;
}
