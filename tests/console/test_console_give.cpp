/**
 * Host-only tests for the `give` console command's pure resolution layer
 * (SOURCES/CONSOLE/CONSOLE_GIVE.CPP). No retail data, no engine init.
 *
 * Intent (must stay aligned with production code):
 * - cmd_give (CONSOLE_CMD.CPP) delegates all item-name resolution, count
 *   parsing and validation to Console_GiveParse / Console_GiveResolveItem.
 * - Indices here mirror COMMON.H FLAG_*; CONSOLE_CMD.CPP #error-guards them.
 */

#include "CONSOLE/CONSOLE_GIVE.H"

#include "test_harness.h"

static void test_resolve_by_name(void) {
    ASSERT_EQ_INT(0, Console_GiveResolveItem("holomap"));
    ASSERT_EQ_INT(21, Console_GiveResolveItem("gem"));
    ASSERT_EQ_INT(22, Console_GiveResolveItem("conch"));
}

static void test_resolve_case_insensitive(void) {
    ASSERT_EQ_INT(22, Console_GiveResolveItem("CoNcH"));
    ASSERT_EQ_INT(22, Console_GiveResolveItem("CONCH"));
}

static void test_resolve_numeric_fallback(void) {
    ASSERT_EQ_INT(0, Console_GiveResolveItem("0"));
    ASSERT_EQ_INT(22, Console_GiveResolveItem("22"));
    ASSERT_EQ_INT(39, Console_GiveResolveItem("39"));
}

static void test_resolve_numeric_out_of_range(void) {
    ASSERT_EQ_INT(-1, Console_GiveResolveItem("40"));  /* scaphandre — outside inventory */
    ASSERT_EQ_INT(-1, Console_GiveResolveItem("251")); /* clover — name-only */
    ASSERT_EQ_INT(-1, Console_GiveResolveItem("999"));
    ASSERT_EQ_INT(-1, Console_GiveResolveItem("-1"));
}

static void test_resolve_alias(void) {
    ASSERT_EQ_INT(24, Console_GiveResolveItem("routedisc"));
    ASSERT_EQ_INT(24, Console_GiveResolveItem("acfviewer"));
}

static void test_resolve_clover_by_name(void) {
    ASSERT_EQ_INT(CONSOLE_GIVE_IDX_CLOVER, Console_GiveResolveItem("clover"));
}

static void test_resolve_unknown(void) {
    ASSERT_EQ_INT(-1, Console_GiveResolveItem("banana"));
    ASSERT_EQ_INT(-1, Console_GiveResolveItem(""));
    ASSERT_EQ_INT(-1, Console_GiveResolveItem("12abc"));
}

static void test_is_count_item(void) {
    ASSERT_TRUE(Console_GiveIsCountItem(2));  /* darts */
    ASSERT_TRUE(Console_GiveIsCountItem(8));  /* money */
    ASSERT_TRUE(Console_GiveIsCountItem(14)); /* penguin */
    ASSERT_TRUE(Console_GiveIsCountItem(21)); /* gem */
    ASSERT_TRUE(Console_GiveIsCountItem(CONSOLE_GIVE_IDX_CLOVER));
    ASSERT_TRUE(!Console_GiveIsCountItem(0));  /* holomap */
    ASSERT_TRUE(!Console_GiveIsCountItem(22)); /* conch */
}

static void test_item_name(void) {
    ASSERT_TRUE(strcmp(Console_GiveItemName(8), "money") == 0);
    ASSERT_TRUE(strcmp(Console_GiveItemName(22), "conch") == 0);
    ASSERT_TRUE(Console_GiveItemName(40) == NULL); /* not give-able */
}

static void test_parse_boolean_ok(void) {
    const char *argv[] = {"give", "conch"};
    S32 idx = -99, cnt = -99;
    const char *err = "unset";
    ASSERT_TRUE(Console_GiveParse(2, argv, &idx, &cnt, &err));
    ASSERT_EQ_INT(22, idx);
    ASSERT_EQ_INT(1, cnt);
}

static void test_parse_count_ok(void) {
    const char *argv[] = {"give", "gem", "5"};
    S32 idx = -99, cnt = -99;
    const char *err = NULL;
    ASSERT_TRUE(Console_GiveParse(3, argv, &idx, &cnt, &err));
    ASSERT_EQ_INT(21, idx);
    ASSERT_EQ_INT(5, cnt);
}

static void test_parse_missing_item(void) {
    const char *argv[] = {"give"};
    S32 idx = -99, cnt = -99;
    const char *err = NULL;
    ASSERT_TRUE(!Console_GiveParse(1, argv, &idx, &cnt, &err));
    ASSERT_TRUE(err != NULL);
}

static void test_parse_count_on_boolean(void) {
    const char *argv[] = {"give", "conch", "3"};
    S32 idx = -99, cnt = -99;
    const char *err = NULL;
    ASSERT_TRUE(!Console_GiveParse(3, argv, &idx, &cnt, &err));
    ASSERT_TRUE(err != NULL);
}

static void test_parse_count_too_low(void) {
    const char *argv[] = {"give", "gem", "0"};
    S32 idx = -99, cnt = -99;
    const char *err = NULL;
    ASSERT_TRUE(!Console_GiveParse(3, argv, &idx, &cnt, &err));
    ASSERT_TRUE(err != NULL);
}

static void test_parse_count_not_a_number(void) {
    const char *argv[] = {"give", "gem", "lots"};
    S32 idx = -99, cnt = -99;
    const char *err = NULL;
    ASSERT_TRUE(!Console_GiveParse(3, argv, &idx, &cnt, &err));
    ASSERT_TRUE(err != NULL);
}

static void test_parse_unknown_item(void) {
    const char *argv[] = {"give", "banana"};
    S32 idx = -99, cnt = -99;
    const char *err = NULL;
    ASSERT_TRUE(!Console_GiveParse(2, argv, &idx, &cnt, &err));
    ASSERT_TRUE(err != NULL);
}

int main(void) {
    RUN_TEST(test_resolve_by_name);
    RUN_TEST(test_resolve_case_insensitive);
    RUN_TEST(test_resolve_numeric_fallback);
    RUN_TEST(test_resolve_numeric_out_of_range);
    RUN_TEST(test_resolve_alias);
    RUN_TEST(test_resolve_clover_by_name);
    RUN_TEST(test_resolve_unknown);
    RUN_TEST(test_is_count_item);
    RUN_TEST(test_item_name);
    RUN_TEST(test_parse_boolean_ok);
    RUN_TEST(test_parse_count_ok);
    RUN_TEST(test_parse_missing_item);
    RUN_TEST(test_parse_count_on_boolean);
    RUN_TEST(test_parse_count_too_low);
    RUN_TEST(test_parse_count_not_a_number);
    RUN_TEST(test_parse_unknown_item);
    TEST_SUMMARY();
    return test_failures != 0;
}
