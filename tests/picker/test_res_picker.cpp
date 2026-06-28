/**
 * Host-only tests for the first-launch folder-picker (no Docker, no retail
 * data, no display).
 *
 * The picker UI itself can't be driven headless, but two things it depends on
 * can be locked down here:
 *
 *   1. Message-box wording, held to a contract: no em-dash, no pointer to an
 *      unshipped .md doc (release packages carry README.txt, not docs/), a
 *      length ceiling (the "don't grow a README inside a modal" guard), and the
 *      recovery tokens each message promises to the user.
 *
 *   2. The platform gate. Off-Android, Android_GetExternalFilesDir is a no-op
 *      returning 0, so PromptForResDir never takes the Android branch and
 *      reaches the desktop picker. Lock that invariant so a stub change can't
 *      silently divert desktop builds into the Android copy-data message.
 */

#include "RES_PICKER.H"

#include <SYSTEM/ANDROID.H>
#include <SYSTEM/LIMITS.H>

#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void check(bool cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

/* The em-dash, U+2014, is 0xE2 0x80 0x94 in UTF-8. The repo's commit hook
   blocks newly-added ones in source; this is the user-facing-string backstop so
   a message body can't regress one in unnoticed. */
static bool HasEmDash(const char *s) {
    return strstr(s, "\xe2\x80\x94") != NULL;
}

/* Ceiling 350 keeps every box well clear of "a README in a modal"; the longest
   today (no-picker) is ~220 chars. */
static const size_t kMaxLen = 350;

static void check_message(const char *name, const char *body) {
    char ctx[128];

    snprintf(ctx, sizeof ctx, "%s: no em-dash", name);
    check(!HasEmDash(body), ctx);

    /* Release artifacts ship README.txt, not the .md docs, so a user-facing
       dialog must never point the player at a docs/*.md file they don't have. */
    snprintf(ctx, sizeof ctx, "%s: no .md pointer (not shipped with release)",
             name);
    check(strstr(body, ".md") == NULL, ctx);

    snprintf(ctx, sizeof ctx, "%s: under length ceiling (%zu)", name, kMaxLen);
    check(strlen(body) <= kMaxLen, ctx);
}

int main(void) {
    /* Wording contract: no em-dash, no unshipped-doc pointer, length ceiling. */
    check_message("DataNotFound", kPickerMsgDataNotFound);
    check_message("InvalidPick", kPickerMsgInvalidPick);
    check_message("NoPicker", kPickerMsgNoPicker);
    check_message("AndroidData", kPickerMsgAndroidData);

    /* Each message must carry the action it promises. */
    check(strstr(kPickerMsgDataNotFound, "lba2.hqr") != NULL,
          "DataNotFound: names lba2.hqr");
    check(strstr(kPickerMsgInvalidPick, "lba2.hqr") != NULL,
          "InvalidPick: names lba2.hqr");
    check(strstr(kPickerMsgNoPicker, "--game-dir") != NULL,
          "NoPicker: offers --game-dir");
    check(strstr(kPickerMsgNoPicker, "LBA2_GAME_DIR") != NULL,
          "NoPicker: offers LBA2_GAME_DIR");
    check(strstr(kPickerMsgNoPicker, "README.txt") != NULL,
          "NoPicker: points at README.txt");
    check(strstr(kPickerMsgAndroidData, "/sdcard/lba2cc/") != NULL,
          "AndroidData: names the copy target");

    /* Off-Android, the gate must fall through to the desktop picker:
       Android_GetExternalFilesDir is a no-op returning 0 here. */
    char buf[ADELINE_MAX_PATH];
    check(Android_GetExternalFilesDir(buf, ADELINE_MAX_PATH) == 0,
          "off-Android: external-files probe returns 0 (gate falls through)");

    if (g_failures) {
        fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
