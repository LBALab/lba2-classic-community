/* Host test for what the player is told when the program ends
 * (SOURCES/BOOT_END.H). Header-only: no engine sources to link, no boot, and
 * nothing has to fail for the failure paths to be reachable.
 *
 * That is the point of it. These messages could previously only be seen by
 * breaking an install: deleting an HQR to read the missing-data text, or
 * exhausting memory to read the other one. Nobody does that on a change to the
 * exit path, so the strings and the decision to raise a dialog at all were
 * unguarded.
 */
#include "BOOT_END.H"
#include "test_harness.h"

#include <string.h>

/* The three failures are shown; a clean exit is not. */
static void test_only_the_failures_are_reported(void) {
    ASSERT_TRUE(BootEnd_IsFailure(BOOT_END_FILE_NOT_FOUND));
    ASSERT_TRUE(BootEnd_IsFailure(BOOT_END_NAME_NOT_FOUND));
    ASSERT_TRUE(BootEnd_IsFailure(BOOT_END_NOT_ENOUGH_MEM));
    ASSERT_EQ_INT(0, BootEnd_IsFailure(BOOT_END_PROGRAM_OK));
}

/* An early exit leaves the code at -1. Raising a dialog for it would put one in
 * front of every player who quits before the code is set, so it is not a
 * failure, and neither is anything else unrecognised. */
static void test_an_unset_code_is_not_a_failure(void) {
    S32 code;
    S32 bad = 0;

    ASSERT_EQ_INT(0, BootEnd_IsFailure(-1));
    for (code = 4; code < 64; code++) {
        if (BootEnd_IsFailure(code))
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

/* Every code the player is shown has something to say, and every code that is
 * not says nothing. A headline without a failure would be a dialog nobody
 * raises; a failure without one would raise an empty dialog. */
static void test_a_headline_exists_exactly_for_a_failure(void) {
    S32 code;
    S32 bad = 0;

    for (code = -2; code < 64; code++) {
        const S32 failure = BootEnd_IsFailure(code);
        const S32 hasText = BootEnd_Headline(code) != NULL;
        const S32 hasLog = BootEnd_LogFormat(code) != NULL;
        if (failure != hasText || failure != hasLog)
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

/* Each failure says a different thing. One shared message would tell a player
 * with a missing file to go looking for memory. */
static void test_each_failure_says_something_of_its_own(void) {
    const char *a = BootEnd_Headline(BOOT_END_FILE_NOT_FOUND);
    const char *b = BootEnd_Headline(BOOT_END_NAME_NOT_FOUND);
    const char *c = BootEnd_Headline(BOOT_END_NOT_ENOUGH_MEM);

    ASSERT_TRUE(strcmp(a, b) != 0);
    ASSERT_TRUE(strcmp(a, c) != 0);
    ASSERT_TRUE(strcmp(b, c) != 0);
}

/* The log line takes the detail as an argument rather than pasting it into the
 * format, so a filename containing a percent sign cannot be read as a
 * conversion. Checked by looking for exactly one, since a second would consume
 * an argument nobody passes. */
static void test_the_log_format_takes_one_argument(void) {
    S32 code;
    S32 bad = 0;

    for (code = 0; code < 4; code++) {
        const char *fmt = BootEnd_LogFormat(code);
        if (fmt == NULL)
            continue;
        if (strstr(fmt, "%s") == NULL || strchr(fmt, '%') != strstr(fmt, "%s"))
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

/* The dialog body carries all three things a stuck player needs: what happened,
 * which thing, and where to look. */
static void test_the_message_carries_what_where_and_why(void) {
    char buf[512];
    const S32 n = BootEnd_FormatMessage(buf, sizeof buf, BOOT_END_FILE_NOT_FOUND, "LBA2.HQR",
                                        "/home/p/.local/share/adeline.log");

    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(buf, "Game data is missing") != NULL);
    ASSERT_TRUE(strstr(buf, "LBA2.HQR") != NULL);
    ASSERT_TRUE(strstr(buf, "adeline.log") != NULL);
    ASSERT_TRUE(strstr(buf, "can't continue") != NULL);
}

/* A code with nothing to report writes nothing and says so, so a caller can use
 * the return rather than asking twice and getting a different answer. */
static void test_nothing_to_report_writes_nothing(void) {
    char buf[64];

    memset(buf, 'x', sizeof buf);
    ASSERT_EQ_INT(0, BootEnd_FormatMessage(buf, sizeof buf, BOOT_END_PROGRAM_OK, "d", "p"));
    ASSERT_EQ_INT(0, (S32)buf[0]);
}

/* A missing detail or log path is written as empty rather than dropped: a dialog
 * short of a line reads as a different failure than the one that happened. */
static void test_a_missing_part_still_leaves_a_whole_message(void) {
    char buf[512];

    ASSERT_TRUE(BootEnd_FormatMessage(buf, sizeof buf, BOOT_END_NOT_ENOUGH_MEM, NULL, NULL) > 0);
    ASSERT_TRUE(strstr(buf, "ran out of memory") != NULL);
    ASSERT_TRUE(strstr(buf, "can't continue") != NULL);
}

/* A buffer too small truncates rather than overruns, and stays a C string. The
 * path is in the message, so the length is whatever the player's home directory
 * happens to be. */
static void test_a_short_buffer_truncates_safely(void) {
    char buf[16];
    S32 i;

    memset(buf, 'x', sizeof buf);
    BootEnd_FormatMessage(buf, sizeof buf, BOOT_END_FILE_NOT_FOUND, "a-very-long-file-name.hqr",
                          "/a/very/long/path/to/the/log/file.log");
    for (i = 0; i < (S32)sizeof buf; i++) {
        if (buf[i] == 0)
            break;
    }
    ASSERT_TRUE(i < (S32)sizeof buf); /* terminated inside the buffer */
}

/* Refuses a buffer it cannot write to rather than writing anyway. */
static void test_no_buffer_is_survivable(void) {
    char buf[8];

    ASSERT_EQ_INT(0, BootEnd_FormatMessage(NULL, 16, BOOT_END_FILE_NOT_FOUND, "d", "p"));
    ASSERT_EQ_INT(0, BootEnd_FormatMessage(buf, 0, BOOT_END_FILE_NOT_FOUND, "d", "p"));
}

int main(void) {
    RUN_TEST(test_only_the_failures_are_reported);
    RUN_TEST(test_an_unset_code_is_not_a_failure);
    RUN_TEST(test_a_headline_exists_exactly_for_a_failure);
    RUN_TEST(test_each_failure_says_something_of_its_own);
    RUN_TEST(test_the_log_format_takes_one_argument);
    RUN_TEST(test_the_message_carries_what_where_and_why);
    RUN_TEST(test_nothing_to_report_writes_nothing);
    RUN_TEST(test_a_missing_part_still_leaves_a_whole_message);
    RUN_TEST(test_a_short_buffer_truncates_safely);
    RUN_TEST(test_no_buffer_is_survivable);
    TEST_SUMMARY();
    return test_failures != 0;
}
