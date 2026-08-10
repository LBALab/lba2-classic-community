/* Host test for EqualsIgnoringCaseAndEncoding (LIB386/SYSTEM/STRING.CPP).
 *
 * Config files do not agree on an encoding. This source tree is UTF-8, so the
 * language table in MESSAGE.CPP spells ç as C3 A7; a retail LBA2.CFG is
 * CP437/CP850 and spells it 87. Matching one against the other with strcasecmp
 * fails, silently, and the caller lands on whatever its fallback happens to be.
 *
 * The bytes below are written as escapes rather than literals so the test says
 * what it means regardless of the encoding this file is saved in.
 */
#include "test_harness.h"

#include <SYSTEM/STRING.H>

/* The names this actually has to match, in the encodings they arrive in. Each
 * escape is broken out of the literal that follows it: a hex escape is greedy,
 * so "\xA7ais" would read the 'a' as a third hex digit. */
#define FR_UTF8 "Fran\xC3\xA7" \
                "ais"
#define FR_CP850 "Fran\x87" \
                 "ais"
#define FR_LATIN1 "Fran\xE7" \
                  "ais"
#define ES_UTF8 "Espa\xC3\xB1" \
                "ol"
#define ES_CP850 "Espa\xA4" \
                 "ol"

static void test_same_encoding_still_matches(void) {
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding("English", "English"));
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding(FR_UTF8, FR_UTF8));
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding(ES_CP850, ES_CP850));
}

static void test_case_is_ignored(void) {
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding("english", "ENGLISH"));
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding("fran"
                                              "\xC3\xA7"
                                              "AIS",
                                              FR_UTF8));
}

/* The point of the helper: an 8-bit config value against a UTF-8 table. */
static void test_across_encodings(void) {
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding(FR_CP850, FR_UTF8));
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding(FR_UTF8, FR_CP850));
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding(FR_LATIN1, FR_UTF8));
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding(ES_CP850, ES_UTF8));
}

/* An accent is not a plain letter: "Francais" is a different string, and
 * accepting it would be a guess rather than a decoding. */
static void test_plain_letter_is_not_an_accent(void) {
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding("Francais", FR_UTF8));
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding("Espanol", ES_CP850));
}

static void test_different_names_stay_different(void) {
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding("English", "Deutsch"));
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding(FR_CP850, ES_UTF8));
    /* A prefix is not a match, in either direction. */
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding("Engl", "English"));
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding("English", "Engl"));
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding(FR_CP850, "Fran\x87"));
}

/* An unset config key arrives as "", and must not match a real name -- that is
 * what tells the caller to report the problem and use its default. */
static void test_empty_and_null(void) {
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding("", ""));
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding("", "English"));
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding("English", ""));
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding(NULL, NULL));
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding(NULL, "English"));
}

/* A trailing accent run has no ASCII after it to resynchronise on, so the
 * lengths only agree if both sides ended together. */
static void test_trailing_accent_run(void) {
    ASSERT_TRUE(EqualsIgnoringCaseAndEncoding("caf\xC3\xA9", "caf\x82"));
    ASSERT_TRUE(!EqualsIgnoringCaseAndEncoding("caf\xC3\xA9", "caf\x82s"));
}

int main(void) {
    RUN_TEST(test_same_encoding_still_matches);
    RUN_TEST(test_case_is_ignored);
    RUN_TEST(test_across_encodings);
    RUN_TEST(test_plain_letter_is_not_an_accent);
    RUN_TEST(test_different_names_stay_different);
    RUN_TEST(test_empty_and_null);
    RUN_TEST(test_trailing_accent_run);
    TEST_SUMMARY();
    return test_failures != 0;
}
