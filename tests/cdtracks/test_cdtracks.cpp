/* Host regression test for the CD-track mapping (LIB386/SYSTEM/CDTRACKS.CPP).
 *
 * On a disc that carries its soundtrack as Red Book audio (the US retail CD),
 * the first seven music entries are CD tracks 2 through 8 rather than files.
 * The mapping decides which sectors a music request plays, so getting it wrong
 * is silent: every scene gets the wrong theme and nothing errors.
 *
 * Two things are pinned here. The lookup itself (names, bounds, path and case
 * handling), and that the table still agrees with SOURCES/MUSIC.CPP, which is
 * where the order comes from. The second check reads that source file rather
 * than linking it, so the table can live beside the disc code without the two
 * drifting apart unnoticed.
 */
#include "test_harness.h"

#include <SYSTEM/CDTRACKS.H>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void test_names_by_number(void) {
    ASSERT_TRUE(strcmp(CdTracks_NameForNumber(2), "TADPCM1") == 0);
    ASSERT_TRUE(strcmp(CdTracks_NameForNumber(3), "TADPCM2") == 0);
    ASSERT_TRUE(strcmp(CdTracks_NameForNumber(7), "JADPCM01") == 0);
    ASSERT_TRUE(strcmp(CdTracks_NameForNumber(8), "TADPCM6") == 0);

    /* Track 1 is the data track, and the audio stops at 8. */
    ASSERT_TRUE(CdTracks_NameForNumber(1) == NULL);
    ASSERT_TRUE(CdTracks_NameForNumber(CDTRACKS_LAST_AUDIO + 1) == NULL);
    ASSERT_TRUE(CdTracks_NameForNumber(0) == NULL);
    ASSERT_TRUE(CdTracks_NameForNumber(-1) == NULL);
}

static void test_number_by_name(void) {
    ASSERT_EQ_INT(2, CdTracks_NumberForMusicName("TADPCM1"));
    ASSERT_EQ_INT(4, CdTracks_NumberForMusicName("TADPCM3.WAV"));
    ASSERT_EQ_INT(4, CdTracks_NumberForMusicName("/install/music/tadpcm3.wav"));
    ASSERT_EQ_INT(4, CdTracks_NumberForMusicName("C:\\install\\music\\TADPCM3.WAV"));

    /* JADPCM01 is a jingle name, but on this disc it is CD track 7. Excluding it
       because of the prefix would leave one theme silent. */
    ASSERT_EQ_INT(7, CdTracks_NumberForMusicName("music/JADPCM01.WAV"));

    /* Every other jingle is a file on every release. */
    ASSERT_EQ_INT(0, CdTracks_NumberForMusicName("music/JADPCM02.WAV"));
    ASSERT_EQ_INT(0, CdTracks_NumberForMusicName("music/LOGADPCM.WAV"));
    ASSERT_EQ_INT(0, CdTracks_NumberForMusicName("TADPCM7"));
    ASSERT_EQ_INT(0, CdTracks_NumberForMusicName(""));
    ASSERT_EQ_INT(0, CdTracks_NumberForMusicName(NULL));
}

/* Round-trip: every audio track's name maps back to its own number. */
static void test_round_trip(void) {
    for (int t = CDTRACKS_FIRST_AUDIO; t <= CDTRACKS_LAST_AUDIO; t++) {
        const char *name = CdTracks_NameForNumber(t);
        ASSERT_TRUE(name != NULL);
        if (name)
            ASSERT_EQ_INT(t, CdTracks_NumberForMusicName(name));
    }
}

/* Pull the quoted entries of MUSIC.CPP's ListJingle initialiser, in order. */
static std::vector<std::string> read_list_jingle(void) {
    std::vector<std::string> names;
    std::string path = std::string(TEST_SOURCE_ROOT) + "SOURCES/MUSIC.CPP";
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return names;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        text.append(buf, n);
    fclose(f);

    size_t start = text.find("ListJingle");
    if (start == std::string::npos)
        return names;
    start = text.find('{', start);
    size_t end = text.find('}', start);
    if (start == std::string::npos || end == std::string::npos)
        return names;

    for (size_t i = start; i < end;) {
        size_t q1 = text.find('"', i);
        if (q1 == std::string::npos || q1 > end)
            break;
        size_t q2 = text.find('"', q1 + 1);
        if (q2 == std::string::npos || q2 > end)
            break;
        names.push_back(text.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return names;
}

/* CD track N is ListJingle[N-1]: track 2 is entry 1, the first real name after
   the empty slot that stands in for "no track". If MUSIC.CPP's order ever
   changes, this is the test that says so. */
static void test_agrees_with_music_cpp(void) {
    std::vector<std::string> list = read_list_jingle();
    ASSERT_TRUE(list.size() > (size_t)CDTRACKS_LAST_AUDIO - 1);
    if (list.size() <= (size_t)CDTRACKS_LAST_AUDIO - 1)
        return;
    ASSERT_TRUE(list[0].empty()); /* index 0 is the "no track" placeholder */

    for (int t = CDTRACKS_FIRST_AUDIO; t <= CDTRACKS_LAST_AUDIO; t++) {
        const char *name = CdTracks_NameForNumber(t);
        ASSERT_TRUE(name != NULL && list[(size_t)t - 1] == name);
    }
}

int main(void) {
    RUN_TEST(test_names_by_number);
    RUN_TEST(test_number_by_name);
    RUN_TEST(test_round_trip);
    RUN_TEST(test_agrees_with_music_cpp);
    TEST_SUMMARY();
    return test_failures != 0;
}
