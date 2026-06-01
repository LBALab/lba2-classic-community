/* Host test for the boot asset preflight (docs/BOOT_LOG_PLAN.md).
 *
 * Builds a fake game-data tree, runs AssetPreflight against it via a capturing
 * file sink, and asserts the required/optional classification, the per-category
 * resolver correctness (video.hqr lives in video/, not the root — a regression
 * that shipped once), and the fail-fast return value. Built with LBA_LOG_NO_SDL
 * so the log core needs no SDL_Init; DIRECTORIES + sys + SDL are linked for the
 * path resolvers, mirroring tests/discovery. */
#include "ASSET_PREFLIGHT.H"
#include "DIRECTORIES.H"
#include "HQR_NAMES.H"
#include <SYSTEM/LOG.H>
#include "test_harness.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#define ROOT "asset_pf_testdir"

static void touch(const char *rel) {
    char p[512];
    snprintf(p, sizeof p, "%s/%s", ROOT, rel);
    FILE *f = fopen(p, "wb");
    if (f) {
        fputc('x', f);
        fclose(f);
    }
}

static void del(const char *rel) {
    char p[512];
    snprintf(p, sizeof p, "%s/%s", ROOT, rel);
    remove(p);
}

static void slurp(const char *path, char *out, int max) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        out[0] = '\0';
        return;
    }
    size_t n = fread(out, 1, (size_t)(max - 1), f);
    out[n] = '\0';
    fclose(f);
}

/* Run the preflight against a fresh capturing file sink; return missing-required
 * count and copy the log text into \a log. */
static int run_preflight(char *log, int max) {
    remove("pf_capture.log");
    Log_Init();
    Log_AddSink(Log_MakeFileSink("pf_capture.log", LOG_DEBUG));
    int r = AssetPreflight();
    Log_Shutdown();
    slurp("pf_capture.log", log, max);
    remove("pf_capture.log");
    return r;
}

static void make_complete_tree(void) {
    SDL_CreateDirectory(ROOT);
    touch("lba2.hqr"); /* IsValidResourceDir marker */
    touch(RESS_HQR_NAME);
    touch(SPRITES_HQR_NAME);
    touch(SPRIRAW_HQR_NAME);
    touch(ANIM_HQR_NAME);
    touch(BODY_HQR_NAME);
    touch(OBJFIX_HQR_NAME);
    touch(TEXT_HQR_NAME);
    touch(SAMPLES_HQR_NAME);
    touch(ANIM3DS_HQR_NAME);
    touch(HOLO_HQR_NAME);
    touch(SCENE_HQR_NAME);
    touch(SCREEN_HQR_NAME);
    touch(BKG_HQR_NAME);
    SDL_CreateDirectory(ROOT "/video");
    touch("video/" VIDEO_HQR_NAME);
    SDL_CreateDirectory(ROOT "/music");
    SDL_CreateDirectory(ROOT "/vox");
}

static char g_log[8192];

static void test_all_present(void) {
    int r = run_preflight(g_log, sizeof g_log);
    ASSERT_EQ_INT(0, r);
    ASSERT_TRUE(strstr(g_log, "all present") != NULL);
}

static void test_optional_missing_does_not_block(void) {
    del(HOLO_HQR_NAME);
    int r = run_preflight(g_log, sizeof g_log);
    ASSERT_EQ_INT(0, r); /* optional missing must not count as required */
    ASSERT_TRUE(strstr(g_log, "holomap.hqr") != NULL);
    touch(HOLO_HQR_NAME); /* restore */
}

static void test_required_missing(void) {
    del(SPRITES_HQR_NAME);
    del("video/" VIDEO_HQR_NAME);
    int r = run_preflight(g_log, sizeof g_log);
    ASSERT_EQ_INT(2, r);
    ASSERT_TRUE(strstr(g_log, "missing required asset: sprites.hqr") != NULL);
    ASSERT_TRUE(strstr(g_log, "missing required asset: video.hqr") != NULL);
    touch(SPRITES_HQR_NAME); /* restore (video restored by next test) */
}

static void test_video_resolved_in_video_dir(void) {
    /* video.hqr must be looked up in video/ (GetMoviePath), not the root: with
     * the file present in video/ it reads as present. A root-only check (the old
     * bug) would report it missing. */
    touch("video/" VIDEO_HQR_NAME);
    int r = run_preflight(g_log, sizeof g_log);
    ASSERT_EQ_INT(0, r);
    ASSERT_TRUE(strstr(g_log, "missing required asset: video.hqr") == NULL);
}

int main(void) {
    make_complete_tree();
    InitDirectories(ROOT "/", ROOT "/", ROOT "/", "", 0);
    RUN_TEST(test_all_present);
    RUN_TEST(test_optional_missing_does_not_block);
    RUN_TEST(test_required_missing);
    RUN_TEST(test_video_resolved_in_video_dir);
    TEST_SUMMARY();
    return test_failures != 0;
}
