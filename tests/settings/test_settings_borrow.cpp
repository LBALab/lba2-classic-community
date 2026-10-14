/* Host test for a replay's borrow of the settings its recording was made under
 * (Settings_Borrow / Settings_ReturnBorrowed in SOURCES/SETTINGS.CPP).
 *
 * The case worth pinning is the one issue #652 names: a borrowed value must never reach
 * the player's lba2.cfg. The exit path is safe by ordering -- the recorder's atexit
 * handler returns the borrow before the one that writes the cfg -- but the options menu
 * and a resolution switch write the cfg mid-session, and a recording that goes through
 * the options menu replays that write. So the writer itself persists what it is to give
 * back, and this checks that through a real cfg file.
 */
#include "SETTINGS.H"
#include "test_harness.h"

#include "CONSOLE/CONSOLE.H"
#include <SYSTEM/DEFFILE.H>

#include <stdio.h>
#include <string.h>

/* SETTINGS.CPP registers cvars through the console, which this test does not link. */
extern "C" void Console_RegisterCvarCoerced(const char *, void *, Console_CvarType, Console_CvarSetFn, void *,
                                            const char *) {}

static const char *kCfgPath = "test_settings_borrow.cfg";
static char g_buffer[16 * 1024];

static S32 g_camera = 0;
static S32 g_told = -1;
static S32 g_toldCount = 0;

static void note_change(S32 value) {
    g_told = value;
    g_toldCount++;
}

static const T_SETTING kRows[] = {
    {"Camera", NULL, NULL, &g_camera, SETTING_TRUTHY, 0, 0, 1, NULL, NULL, note_change, NULL, NULL},
};

static void write_cfg(const char *contents) {
    FILE *f = fopen(kCfgPath, "wb");
    fputs(contents, f);
    fclose(f);
}

static void open_cfg(void) {
    DefFileBufferInit((char *)kCfgPath, g_buffer, (S32)sizeof g_buffer);
}

/* What the file says now, read back through a fresh buffer as the next boot would. */
static S32 cfg_camera(void) {
    open_cfg();
    return DefFileBufferReadValueDefault("Camera", -1);
}

static void reset(const char *cfg) {
    Settings_ReturnBorrowed();
    write_cfg(cfg);
    open_cfg();
    Settings_ReadConfig(kRows, 1);
    g_told = -1;
    g_toldCount = 0;
}

/* The replay runs on the recording's value, and the owner is told, which is how a
   setting with derived state (DetailLevel) takes effect mid-game. */
static void test_a_borrow_installs_and_tells_the_owner(void) {
    reset("Camera: 1\n");
    ASSERT_EQ_INT(1, Settings_Borrow(&kRows[0], 0));
    ASSERT_EQ_INT(0, g_camera);
    ASSERT_EQ_INT(0, g_told);
}

/* The case #652 is about. Without the writer's check this writes 0. */
static void test_a_cfg_written_mid_borrow_keeps_the_players_value(void) {
    reset("Camera: 1\n");
    Settings_Borrow(&kRows[0], 0);
    open_cfg();
    Settings_WriteConfig(kRows, 1);
    ASSERT_EQ_INT(1, cfg_camera());
    ASSERT_EQ_INT(0, g_camera); /* and the replay is still on the recording's */
}

/* The end of the replay puts the player's value back and tells the owner again. */
static void test_the_return_restores_and_tells_the_owner(void) {
    reset("Camera: 1\n");
    Settings_Borrow(&kRows[0], 0);
    Settings_ReturnBorrowed();
    ASSERT_EQ_INT(1, g_camera);
    ASSERT_EQ_INT(1, g_told);
    ASSERT_EQ_INT(2, g_toldCount);
}

/* A second borrow of the same row must not take the first one's value as the
   player's, or the return would leave the recording's value behind. */
static void test_a_second_borrow_keeps_the_first_ones_value(void) {
    reset("Camera: 1\n");
    Settings_Borrow(&kRows[0], 0);
    Settings_Borrow(&kRows[0], 1);
    Settings_Borrow(&kRows[0], 0);
    Settings_ReturnBorrowed();
    ASSERT_EQ_INT(1, g_camera);
}

/* The value is coerced by the row's own rule, as the cfg and the console are. */
static void test_a_borrowed_value_is_coerced(void) {
    reset("Camera: 0\n");
    Settings_Borrow(&kRows[0], 7);
    ASSERT_EQ_INT(1, g_camera);
}

/* Once returned, the writer persists the live value again: the check is for the
   borrow, not a value frozen for the rest of the run. */
static void test_after_the_return_the_writer_persists_the_live_value(void) {
    reset("Camera: 1\n");
    Settings_Borrow(&kRows[0], 0);
    Settings_ReturnBorrowed();
    g_camera = 0;
    open_cfg();
    Settings_WriteConfig(kRows, 1);
    ASSERT_EQ_INT(0, cfg_camera());
}

int main(void) {
    RUN_TEST(test_a_borrow_installs_and_tells_the_owner);
    RUN_TEST(test_a_cfg_written_mid_borrow_keeps_the_players_value);
    RUN_TEST(test_the_return_restores_and_tells_the_owner);
    RUN_TEST(test_a_second_borrow_keeps_the_first_ones_value);
    RUN_TEST(test_a_borrowed_value_is_coerced);
    RUN_TEST(test_after_the_return_the_writer_persists_the_live_value);
    Settings_ReturnBorrowed();
    remove(kCfgPath);
    TEST_SUMMARY();
    return test_failures != 0;
}
