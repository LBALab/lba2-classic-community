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
#include "RETAILFIX.H"
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

/* A hook that writes a second setting, the way a retail fix's row switches the
   master row to Custom (RETAILFIX.CPP). The borrow gives back only the rows it
   borrowed, so the hook has to stand down while it is told about one. */
static S32 g_master = 1;
static S32 g_fix = 1;
static S32 g_borrowingSeen = -1;

static void master_to_custom(S32 value);

static const T_SETTING kLinked[] = {
    {"Fix", NULL, NULL, &g_fix, SETTING_TRUTHY, 1, 0, 1, NULL, NULL, master_to_custom, NULL, NULL},
    {"Master", NULL, NULL, &g_master, SETTING_OR_DEFAULT, 1, 0, 2, NULL, NULL, NULL, NULL, NULL},
};

static void master_to_custom(S32 value) {
    (void)value;
    g_borrowingSeen = Settings_Borrowing();
    if (!Settings_Borrowing())
        Settings_Apply(&kLinked[1], 2);
}

static void reset_linked(void) {
    Settings_ReturnBorrowed();
    write_cfg("Fix: 1\nMaster: 1\n");
    open_cfg();
    Settings_ReadConfig(kLinked, 2);
    g_borrowingSeen = -1;
}

/* Outside a borrow the flag is down, and the hook does its work. */
static void test_a_runtime_write_runs_the_linked_hook(void) {
    reset_linked();
    ASSERT_EQ_INT(0, Settings_Borrowing());
    Settings_Apply(&kLinked[0], 0);
    ASSERT_EQ_INT(0, g_borrowingSeen);
    ASSERT_EQ_INT(2, g_master);
}

/* The trap the flag closes: a replay borrowing the fix row must leave the master
   alone, both ways, and the cfg must keep the player's master. */
static void test_a_borrow_and_its_return_leave_the_linked_row_alone(void) {
    reset_linked();
    Settings_Borrow(&kLinked[0], 0);
    ASSERT_EQ_INT(1, g_borrowingSeen);
    ASSERT_EQ_INT(0, g_fix);
    ASSERT_EQ_INT(1, g_master);
    open_cfg();
    Settings_WriteConfig(kLinked, 2);
    open_cfg();
    ASSERT_EQ_INT(1, DefFileBufferReadValueDefault("Master", -1));

    g_borrowingSeen = -1;
    Settings_ReturnBorrowed();
    ASSERT_EQ_INT(1, g_borrowingSeen);
    ASSERT_EQ_INT(1, g_fix);
    ASSERT_EQ_INT(1, g_master);
    ASSERT_EQ_INT(0, Settings_Borrowing());
}

/* A row added after recordings carried every row: a recording without it
   predates it, and the replay borrows the row's value from before. */
static S32 g_newRow = 1;
static const T_SETTING kAbsent[] = {
    {"NewRow", NULL, NULL, &g_newRow, SETTING_TRUTHY, 1, 0, 1, NULL, NULL, NULL, NULL, NULL, SETTING_RECORDED,
     SETTING_BEFORE(0), SETTING_ABSENT_BEFORE},
    {"NoBefore", NULL, NULL, &g_camera, SETTING_TRUTHY, 0, 0, 1, NULL, NULL, NULL, NULL, NULL, SETTING_RECORDED,
     SETTING_BEFORE_NONE, SETTING_ABSENT_BEFORE},
};

static void test_an_absent_row_borrows_its_value_from_before(void) {
    Settings_ReturnBorrowed();
    g_newRow = 1;
    ASSERT_EQ_INT(1, Settings_BorrowAbsent(&kAbsent[0]));
    ASSERT_EQ_INT(0, g_newRow);
    ASSERT_EQ_INT(1, Settings_BorrowedAbsent(&kAbsent[0]));
    Settings_ReturnBorrowed();
    ASSERT_EQ_INT(1, g_newRow);
    ASSERT_EQ_INT(0, Settings_BorrowedAbsent(&kAbsent[0]));
}

/* No value from before, nothing to borrow: the player's value stays. */
static void test_an_absent_row_without_a_value_from_before_is_left(void) {
    Settings_ReturnBorrowed();
    g_camera = 1;
    ASSERT_EQ_INT(0, Settings_BorrowAbsent(&kAbsent[1]));
    ASSERT_EQ_INT(1, g_camera);
    ASSERT_EQ_INT(0, Settings_BorrowedAbsent(&kAbsent[1]));
}

/* A borrow of a row the recording does carry is not marked absent. */
static void test_an_ordinary_borrow_is_not_marked_absent(void) {
    Settings_ReturnBorrowed();
    g_newRow = 1;
    Settings_Borrow(&kAbsent[0], 0);
    ASSERT_EQ_INT(0, Settings_BorrowedAbsent(&kAbsent[0]));
    Settings_ReturnBorrowed();
}

/* The case it is for: the terrain-seams fix under Fix retail bugs, Yes. A
   recording that predates the fix replays with it off although the master says
   Yes, leaves the master and the other fixes alone, and gives it all back. */
static const T_SETTING *retail_row(T_RETAIL_FIX fix) {
    S32 n = 0;
    const T_SETTING *rows = RetailFix_Settings(&n);
    ASSERT_EQ_INT(RETAIL_FIX_COUNT + 1, n);
    return &rows[fix];
}

static void test_a_fix_the_recording_predates_is_off_under_yes(void) {
    Settings_ReturnBorrowed();
    const T_SETTING *seams = retail_row(RETAIL_FIX_TERRAIN_SEAMS);
    ASSERT_EQ_INT(SETTING_ABSENT_BEFORE, seams->absent);
    ASSERT_EQ_INT(RETAIL_FIXES_YES, RetailFixes);
    ASSERT_EQ_INT(1, RetailFix_On(RETAIL_FIX_TERRAIN_SEAMS));

    ASSERT_EQ_INT(1, Settings_BorrowAbsent(seams));
    ASSERT_EQ_INT(0, RetailFix_On(RETAIL_FIX_TERRAIN_SEAMS));
    ASSERT_EQ_INT(1, RetailFix_On(RETAIL_FIX_FOUND_OBJ_WALK));
    ASSERT_EQ_INT(RETAIL_FIXES_YES, RetailFixes);

    Settings_ReturnBorrowed();
    ASSERT_EQ_INT(1, RetailFix_On(RETAIL_FIX_TERRAIN_SEAMS));
    ASSERT_EQ_INT(RETAIL_FIXES_YES, RetailFixes);
}

/* The found-item fix keeps today's rule: a recording without its row replays on
   the player's value. Only the terrain fix opted in. */
static void test_the_found_item_fix_keeps_the_players_value(void) {
    ASSERT_EQ_INT(SETTING_ABSENT_KEEP, retail_row(RETAIL_FIX_FOUND_OBJ_WALK)->absent);
}

int main(void) {
    RUN_TEST(test_a_borrow_installs_and_tells_the_owner);
    RUN_TEST(test_a_cfg_written_mid_borrow_keeps_the_players_value);
    RUN_TEST(test_the_return_restores_and_tells_the_owner);
    RUN_TEST(test_a_second_borrow_keeps_the_first_ones_value);
    RUN_TEST(test_a_borrowed_value_is_coerced);
    RUN_TEST(test_after_the_return_the_writer_persists_the_live_value);
    RUN_TEST(test_a_runtime_write_runs_the_linked_hook);
    RUN_TEST(test_a_borrow_and_its_return_leave_the_linked_row_alone);
    RUN_TEST(test_an_absent_row_borrows_its_value_from_before);
    RUN_TEST(test_an_absent_row_without_a_value_from_before_is_left);
    RUN_TEST(test_an_ordinary_borrow_is_not_marked_absent);
    RUN_TEST(test_a_fix_the_recording_predates_is_off_under_yes);
    RUN_TEST(test_the_found_item_fix_keeps_the_players_value);
    Settings_ReturnBorrowed();
    remove(kCfgPath);
    TEST_SUMMARY();
    return test_failures != 0;
}
