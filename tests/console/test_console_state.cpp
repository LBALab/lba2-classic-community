/**
 * Host-only tests for console state helpers (no retail game data, no engine init).
 *
 * Intent (must stay aligned with production code):
 * - SOURCES/CONSOLE/CONSOLE_CMD.CPP: console_avail_in_game_scene() delegates to
 *   Console_AvailInGameScene_FromState — same predicate as whether stateful
 *   commands like give may run.
 * - Console_FormatStatusIslandLine_FromState: first line of the status command
 *   (island, cube, chapter); same string as cmd_status (raw globals, no masking).
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

/** Mirrors CONSOLE_STATE.CPP in_scene / CONSOLE_CMD availability (commands may run). */
static int console_in_scene_intent(int flag_acf, const void *scene, S32 cube, S32 phantom) {
    return (scene != NULL) && (cube != phantom) && !flag_acf;
}

static void assert_avail_matches_in_scene_predicate(int flag_acf, const void *scene, S32 cube, S32 phantom) {
    const char *avail = Console_AvailInGameScene_FromState(flag_acf, scene, cube, phantom);
    const int in_scene = console_in_scene_intent(flag_acf, scene, cube, phantom);
    assert((avail == NULL) == (in_scene != 0));
}

int main(void) {
    const char *video_msg = "Not available while a video is playing.";
    const char *no_scene = "Not available until a scene is loaded.";
    const char *phantom = "Not available in this state (not in a loaded cube).";

    expect_avail(1, (void *)1, 0, 94, video_msg);
    expect_avail(0, NULL, 0, 94, no_scene);
    expect_avail(0, (void *)1, 94, 94, phantom);
    expect_avail(0, (void *)1, 5, 94, NULL);

    expect_avail(1, NULL, 94, 94, video_msg);

    assert(strcmp(Console_ModeString_FromState(1, (void *)1, 5, 94), "video") == 0);
    assert(strcmp(Console_ModeString_FromState(0, NULL, 5, 94), "menu") == 0);
    assert(strcmp(Console_ModeString_FromState(0, (void *)1, 94, 94), "menu") == 0);
    assert(strcmp(Console_ModeString_FromState(0, (void *)1, 3, 94), "game") == 0);

    assert_avail_matches_in_scene_predicate(0, (void *)1, 100, 100);
    assert_avail_matches_in_scene_predicate(0, (void *)1, 101, 100);

    char buf[256];
    S16 vars[256];
    memset(vars, 0, sizeof(vars));
    vars[CONSOLE_LISTVAR_FLAG_CHAPTER] = 7;

    Console_FormatStatusIslandLine_FromState(buf, sizeof(buf), 10, 3, vars, CONSOLE_LISTVAR_FLAG_CHAPTER);
    assert(strcmp(buf, "Island: 10  Cube: 3  Chapter: 7") == 0);

    /* status uses raw globals: chapter still shown when not in loaded cube. */
    Console_FormatStatusIslandLine_FromState(buf, sizeof(buf), 0, 99, vars, CONSOLE_LISTVAR_FLAG_CHAPTER);
    assert(strcmp(buf, "Island: 0  Cube: 99  Chapter: 7") == 0);

    Console_FormatStatusIslandLine_FromState(buf, sizeof(buf), 1, 2, NULL, CONSOLE_LISTVAR_FLAG_CHAPTER);
    assert(strcmp(buf, "Island: 1  Cube: 2  Chapter: 0") == 0);

    assert_avail_matches_in_scene_predicate(0, NULL, 1, 94);
    assert_avail_matches_in_scene_predicate(0, (void *)0x1, 94, 94);
    assert_avail_matches_in_scene_predicate(1, (void *)0x1, 3, 94);
    assert_avail_matches_in_scene_predicate(0, (void *)0x1, 0, 94);

    return 0;
}
