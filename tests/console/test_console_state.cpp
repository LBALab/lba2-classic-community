/**
 * Host-only tests for console state helpers (no retail game data, no engine init).
 *
 * Intent (must stay aligned with production code):
 * - SOURCES/CONSOLE/CONSOLE_CMD.CPP: `console_avail_in_game_scene()` delegates to
 *   `Console_AvailInGameScene_FromState` — same predicate as whether stateful
 *   commands like `give` may run.
 * - SOURCES/CONSOLE/CONSOLE.CPP: context on open uses
 *   `Console_FormatContextLine_FromState`, which shows island/cube/chapter as
 *   numbers only when "in scene" — the same boolean as availability (non-NULL
 *   scene, non-phantom cube, not during ACF video).
 */

#include "CONSOLE/CONSOLE_STATE.H"

#include <cassert>
#include <cstdio>
#include <cstring>

static void expect_avail(int flag_acf, const void *scene, S32 cube, S32 phantom, const char *expect_reason) {
    const char *got = Console_AvailInGameScene_FromState(flag_acf, scene, cube, phantom);
    if (expect_reason == NULL) {
        assert(got == NULL);
    } else {
        assert(got != NULL);
        assert(strcmp(got, expect_reason) == 0);
    }
}

/** Mirrors CONSOLE_STATE.CPP `in_scene` / CONSOLE_CMD availability (commands may run). */
static int console_in_scene_intent(int flag_acf, const void *scene, S32 cube, S32 phantom) {
    return (scene != NULL) && (cube != phantom) && !flag_acf;
}

/**
 * For every state, availability NULL iff "in scene"; formatted line shows numeric
 * island (and cube) iff in scene — same coupling as give/context in the real game.
 */
static void assert_avail_matches_context_numbers(int flag_acf, const void *scene, S32 cube, S32 phantom) {
    const char *avail = Console_AvailInGameScene_FromState(flag_acf, scene, cube, phantom);
    const int in_scene = console_in_scene_intent(flag_acf, scene, cube, phantom);
    assert((avail == NULL) == (in_scene != 0));

    char buf[256];
    S16 vars[256];
    memset(vars, 0, sizeof(vars));
    vars[CONSOLE_LISTVAR_FLAG_CHAPTER] = 88;
    Console_FormatContextLine_FromState(buf, sizeof(buf), flag_acf, scene, cube, phantom, 55, vars,
                                        CONSOLE_LISTVAR_FLAG_CHAPTER);

    if (in_scene) {
        assert(strstr(buf, "island=55") != NULL);
        assert(strstr(buf, "cube=") != NULL);
        /* cube number in line must match requested cube (e.g. cube=3) */
        char expect_cube[32];
        snprintf(expect_cube, sizeof(expect_cube), "cube=%d", (int)cube);
        assert(strstr(buf, expect_cube) != NULL);
        assert(strstr(buf, "chapter=88") != NULL);
    } else {
        assert(strstr(buf, "island=-") != NULL);
        assert(strstr(buf, "cube=-") != NULL);
        assert(strstr(buf, "chapter=-") != NULL);
    }
}

int main(void) {
    const char *video_msg = "Not available while a video is playing.";
    const char *no_scene = "Not available until a scene is loaded.";
    const char *phantom = "Not available in this state (not in a loaded cube).";

    expect_avail(1, (void *)1, 0, 94, video_msg);
    expect_avail(0, NULL, 0, 94, no_scene);
    expect_avail(0, (void *)1, 94, 94, phantom);
    expect_avail(0, (void *)1, 5, 94, NULL);

    /* Video blocks before "no scene" — same order as CONSOLE_STATE.CPP */
    expect_avail(1, NULL, 94, 94, video_msg);

    assert(strcmp(Console_ModeString_FromState(1, (void *)1, 5, 94), "video") == 0);
    assert(strcmp(Console_ModeString_FromState(0, NULL, 5, 94), "menu") == 0);
    assert(strcmp(Console_ModeString_FromState(0, (void *)1, 94, 94), "menu") == 0);
    assert(strcmp(Console_ModeString_FromState(0, (void *)1, 3, 94), "game") == 0);

    /* Phantom id is a parameter (game uses COMMON.H NUM_CUBE_PHANTOM). */
    assert_avail_matches_context_numbers(0, (void *)1, 100, 100);
    assert_avail_matches_context_numbers(0, (void *)1, 101, 100);

    char buf[256];
    S16 vars[256];
    memset(vars, 0, sizeof(vars));
    vars[CONSOLE_LISTVAR_FLAG_CHAPTER] = 7;

    Console_FormatContextLine_FromState(buf, sizeof(buf), 0, (void *)1, 3, 94, 42, vars, CONSOLE_LISTVAR_FLAG_CHAPTER);
    assert(strstr(buf, "mode=game") != NULL);
    assert(strstr(buf, "island=42") != NULL);
    assert(strstr(buf, "cube=3") != NULL);
    assert(strstr(buf, "chapter=7") != NULL);

    /* Menu: stale island/cube/chapter must not leak as numbers */
    Console_FormatContextLine_FromState(buf, sizeof(buf), 0, NULL, 99, 94, 12345, vars, CONSOLE_LISTVAR_FLAG_CHAPTER);
    assert(strstr(buf, "mode=menu") != NULL);
    assert(strstr(buf, "island=-") != NULL);
    assert(strstr(buf, "cube=-") != NULL);
    assert(strstr(buf, "chapter=-") != NULL);

    /* Video: hide numeric scene fields (same in_scene predicate as availability) */
    Console_FormatContextLine_FromState(buf, sizeof(buf), 1, (void *)1, 3, 94, 1, vars, CONSOLE_LISTVAR_FLAG_CHAPTER);
    assert(strstr(buf, "mode=video") != NULL);
    assert(strstr(buf, "island=-") != NULL);

    /* In-scene but no ListVarGame pointer: island/cube numeric, chapter '-' (formatter only). */
    Console_FormatContextLine_FromState(buf, sizeof(buf), 0, (void *)1, 2, 94, 9, NULL, CONSOLE_LISTVAR_FLAG_CHAPTER);
    assert(Console_AvailInGameScene_FromState(0, (void *)1, 2, 94) == NULL);
    assert(strstr(buf, "island=9") != NULL);
    assert(strstr(buf, "cube=2") != NULL);
    assert(strstr(buf, "chapter=-") != NULL);

    /* Grid: avail NULL iff context shows numeric island (coupling intent). */
    assert_avail_matches_context_numbers(0, NULL, 1, 94);
    assert_avail_matches_context_numbers(0, (void *)0x1, 94, 94);
    assert_avail_matches_context_numbers(1, (void *)0x1, 3, 94);
    assert_avail_matches_context_numbers(0, (void *)0x1, 0, 94);

    return 0;
}
