#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/CD_PATH.H"

#include <string.h>

static void test_directory_separator() {
    struct Case {
        const char *directory;
        const char *music;
        int track;
        const char *expected;
    };
    const Case cases[] = {
        {"/game/data", "music", 6, "/game/data/music/Track06.wav"},
        {"/game/data/", "Music", 2, "/game/data/Music/Track02.wav"},
        {"/", "MUSIC", 12, "/MUSIC/Track12.wav"},
        {"C:\\game\\data", "music", 6, "C:\\game\\data/music/Track06.wav"},
        {"C:\\game\\data\\", "music", 6, "C:\\game\\data\\music/Track06.wav"},
        {"C:\\", "Music", 2, "C:\\Music/Track02.wav"},
        {"\\\\server\\share", "music", 6, "\\\\server\\share/music/Track06.wav"},
        {"relative", "MUSIC", 100, "relative/MUSIC/Track100.wav"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char output[256];
        ASSERT_TRUE(SDLCD_FormatTrackPath(output, sizeof(output), cases[i].directory, cases[i].music, cases[i].track));
        ASSERT_TRUE(strcmp(output, cases[i].expected) == 0);
    }
}

static void test_capacity_and_invalid_inputs() {
    const char *expected = "/music/Track06.wav";
    char output[256];
    const int exact = (int)strlen(expected) + 1;
    ASSERT_TRUE(SDLCD_FormatTrackPath(output, exact, "/", "music", 6));
    ASSERT_TRUE(strcmp(output, expected) == 0);
    ASSERT_TRUE(!SDLCD_FormatTrackPath(output, exact - 1, "/", "music", 6));
    ASSERT_EQ_INT(0, output[0]);
    ASSERT_TRUE(!SDLCD_FormatTrackPath(output, sizeof(output), "", "music", 6));
    ASSERT_EQ_INT(0, output[0]);
    ASSERT_TRUE(!SDLCD_FormatTrackPath(output, sizeof(output), NULL, "music", 6));
    ASSERT_TRUE(!SDLCD_FormatTrackPath(output, sizeof(output), "/", NULL, 6));
    ASSERT_TRUE(!SDLCD_FormatTrackPath(output, 0, "/", "music", 6));
    ASSERT_TRUE(!SDLCD_FormatTrackPath(NULL, sizeof(output), "/", "music", 6));
}

int main() {
    RUN_TEST(test_directory_separator);
    RUN_TEST(test_capacity_and_invalid_inputs);
    TEST_SUMMARY();
    return test_failures != 0;
}
