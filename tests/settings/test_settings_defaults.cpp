/* Host test for Settings_MoveDefaults (SOURCES/SETTINGS.CPP): a default changed after a
 * release wrote the old one into players' lba2.cfg.
 *
 * The writer persists every row, so a player's file holds the release's defaults as if
 * chosen. A row the player's own file still holds at the old default moves to the new
 * one, once; a value they changed, one an install's data layer supplied, and a file
 * already written under the new defaults are left alone. Checked through real cfg files.
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

static const char *kCfgPath = "test_settings_defaults.cfg";
static const char *kLayerPath = "test_settings_defaults_layer.cfg";
static char g_buffer[16 * 1024];

static S32 g_anchor = 0;
static S32 g_blend = 0;

/* The new defaults: anchor 1 (was 0), blend 900 (was 600). */
static const T_SETTING kRows[] = {
    {"Anchor", NULL, NULL, &g_anchor, SETTING_TRUTHY, 1, 0, 1, NULL, NULL, NULL, NULL, NULL},
    {"Blend", NULL, NULL, &g_blend, SETTING_CLAMP, 900, 0, 3000, NULL, NULL, NULL, NULL, NULL},
};

static const T_SETTING_DEFAULT_MOVE kMoves[] = {
    {"Anchor", 0, 1},
    {"Blend", 600, 1},
};

static void write_file(const char *path, const char *contents) {
    FILE *f = fopen(path, "wb");
    fputs(contents, f);
    fclose(f);
}

/* Boot: read the rows, then move what the file holds at an older default. */
static S32 boot(const char *cfg, const char *layer) {
    write_file(kCfgPath, cfg);
    DefFileBufferInit((char *)kCfgPath, g_buffer, (S32)sizeof g_buffer);
    if (layer != NULL) {
        write_file(kLayerPath, layer);
        DefFileBufferAppendFile(kLayerPath);
    }
    Settings_ReadConfig(kRows, 2);
    return Settings_MoveDefaults(kRows, 2, "Defaults", 1, kMoves, 2);
}

/* A v0.13.2 file: both rows at the old defaults, no marker. */
static void test_a_file_at_the_old_defaults_moves_to_the_new_ones(void) {
    ASSERT_EQ_INT(2, boot("Anchor: 0\nBlend: 600\n", NULL));
    ASSERT_EQ_INT(1, g_anchor);
    ASSERT_EQ_INT(900, g_blend);
}

/* A value the player changed is theirs. */
static void test_a_changed_value_is_kept(void) {
    ASSERT_EQ_INT(1, boot("Anchor: 0\nBlend: 750\n", NULL));
    ASSERT_EQ_INT(1, g_anchor);
    ASSERT_EQ_INT(750, g_blend);
}

/* Written under the new defaults: what it holds was chosen there, old default or not. */
static void test_a_file_with_the_marker_moves_nothing(void) {
    ASSERT_EQ_INT(0, boot("Anchor: 0\nBlend: 600\nDefaults: 1\n", NULL));
    ASSERT_EQ_INT(0, g_anchor);
    ASSERT_EQ_INT(600, g_blend);
}

/* An install's data layer is not the player's choice, and not what moves. */
static void test_a_value_from_the_data_layer_is_left(void) {
    ASSERT_EQ_INT(0, boot("Blend: 750\n", "Anchor: 0\n"));
    ASSERT_EQ_INT(0, g_anchor);
    ASSERT_EQ_INT(750, g_blend);
}

/* Nor does a marker in the data layer speak for the player's file. */
static void test_a_marker_in_the_data_layer_does_not_count(void) {
    ASSERT_EQ_INT(2, boot("Anchor: 0\nBlend: 600\n", "Defaults: 1\n"));
    ASSERT_EQ_INT(1, g_anchor);
    ASSERT_EQ_INT(900, g_blend);
}

/* A file that never held the rows boots on the defaults already. */
static void test_absent_rows_take_the_defaults_without_a_move(void) {
    ASSERT_EQ_INT(0, boot("Other: 3\n", NULL));
    ASSERT_EQ_INT(1, g_anchor);
    ASSERT_EQ_INT(900, g_blend);
}

int main(void) {
    RUN_TEST(test_a_file_at_the_old_defaults_moves_to_the_new_ones);
    RUN_TEST(test_a_changed_value_is_kept);
    RUN_TEST(test_a_file_with_the_marker_moves_nothing);
    RUN_TEST(test_a_value_from_the_data_layer_is_left);
    RUN_TEST(test_a_marker_in_the_data_layer_does_not_count);
    RUN_TEST(test_absent_rows_take_the_defaults_without_a_move);
    remove(kCfgPath);
    remove(kLayerPath);
    TEST_SUMMARY();
    return test_failures != 0;
}
