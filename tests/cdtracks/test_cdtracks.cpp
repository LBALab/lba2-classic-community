/* Host regression test for the CD-track mapping (LIB386/SYSTEM/CDTRACKS.CPP).
 *
 * On a disc that carries its soundtrack as Red Book audio (the US retail CD),
 * six of the themes are CD tracks 2 through 7 rather than files. The mapping
 * decides which sectors a music request plays, so getting it wrong is silent:
 * every scene gets a plausible theme, just not its own.
 *
 * Four things are pinned. The lookup itself (names, bounds, path and case
 * handling); that the table still agrees with SOURCES/MUSIC.CPP's ListJingle,
 * which is where the order comes from; that MUSIC.CPP's two track tables
 * still hold identical music numbers, which is what lets PlayMusic route both
 * releases through one path; and that scripts/dev/disc_extract.py, which names
 * a ripper's output by this same mapping, still spells it the same way. Those
 * three read their source file rather than linking it, so the table can live
 * beside the disc code without the copies drifting apart unnoticed.
 */
#include "test_harness.h"

#include <SYSTEM/CDTRACKS.H>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

static void test_names_by_number(void) {
    ASSERT_TRUE(strcmp(CdTracks_NameForNumber(2), "TADPCM2") == 0);
    ASSERT_TRUE(strcmp(CdTracks_NameForNumber(3), "TADPCM3") == 0);
    ASSERT_TRUE(strcmp(CdTracks_NameForNumber(6), "JADPCM01") == 0);
    ASSERT_TRUE(strcmp(CdTracks_NameForNumber(7), "TADPCM6") == 0);

    /* Track 1 is the data track, and the audio stops at 7. */
    ASSERT_TRUE(CdTracks_NameForNumber(1) == NULL);
    ASSERT_TRUE(CdTracks_NameForNumber(CDTRACKS_LAST_AUDIO + 1) == NULL);
    ASSERT_TRUE(CdTracks_NameForNumber(0) == NULL);
    ASSERT_TRUE(CdTracks_NameForNumber(-1) == NULL);
}

static void test_number_by_name(void) {
    ASSERT_EQ_INT(2, CdTracks_NumberForMusicName("TADPCM2"));
    ASSERT_EQ_INT(3, CdTracks_NumberForMusicName("TADPCM3.WAV"));
    ASSERT_EQ_INT(3, CdTracks_NumberForMusicName("/install/music/tadpcm3.wav"));
    ASSERT_EQ_INT(3, CdTracks_NumberForMusicName("C:\\install\\music\\TADPCM3.WAV"));

    /* JADPCM01 is a jingle name, but on this disc it is CD track 6. Excluding it
       because of the prefix would leave one theme silent. */
    ASSERT_EQ_INT(6, CdTracks_NumberForMusicName("music/JADPCM01.WAV"));

    /* TADPCM1 is the ListJingle[1] slot, which lines up with the data track:
       that piece of music is not on the disc, so it has no CD track. */
    ASSERT_EQ_INT(0, CdTracks_NumberForMusicName("TADPCM1"));

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

/* The CD track number is the music index: track N holds ListJingle[N]. Track 1
   is the data track, so the ListJingle[1] slot has no track of its own. If
   MUSIC.CPP's order ever changes, this is the test that says so. */
static void test_agrees_with_music_cpp(void) {
    std::vector<std::string> list = read_list_jingle();
    ASSERT_TRUE(list.size() > (size_t)CDTRACKS_LAST_AUDIO);
    if (list.size() <= (size_t)CDTRACKS_LAST_AUDIO)
        return;
    ASSERT_TRUE(list[0].empty()); /* index 0 is the "no track" placeholder */

    for (int t = CDTRACKS_FIRST_AUDIO; t <= CDTRACKS_LAST_AUDIO; t++) {
        const char *name = CdTracks_NameForNumber(t);
        ASSERT_TRUE(name != NULL && list[(size_t)t] == name);
    }
}

/* The extractor cuts CD audio out of a rip and names the files, so it carries its
   own copy of this table. A drift there is silent in a way the engine's is not:
   nothing fails, the themes just come out under each other's names. Read its
   `CD_TRACK_NAMES = { 2: "TADPCM2", ... }` and require an exact match. */
static void test_agrees_with_extractor(void) {
    std::string path = std::string(TEST_SOURCE_ROOT) + "scripts/dev/disc_extract.py";
    FILE *f = fopen(path.c_str(), "rb");
    ASSERT_TRUE(f != NULL);
    if (!f)
        return;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        text.append(buf, n);
    fclose(f);

    size_t start = text.find("CD_TRACK_NAMES = {");
    ASSERT_TRUE(start != std::string::npos);
    if (start == std::string::npos)
        return;
    size_t end = text.find('}', start);
    ASSERT_TRUE(end != std::string::npos);
    if (end == std::string::npos)
        return;

    int seen = 0;
    for (size_t i = text.find('{', start) + 1; i < end;) {
        size_t digit = text.find_first_of("0123456789", i);
        if (digit == std::string::npos || digit > end)
            break;
        size_t colon = text.find(':', digit);
        if (colon == std::string::npos || colon > end)
            break;
        size_t q1 = text.find('"', colon);
        size_t q2 = (q1 == std::string::npos) ? q1 : text.find('"', q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos || q2 > end)
            break;
        int track = atoi(text.c_str() + digit);
        std::string name = text.substr(q1 + 1, q2 - q1 - 1);
        const char *mine = CdTracks_NameForNumber(track);
        ASSERT_TRUE(mine != NULL && name == mine);
        seen++;
        i = q2 + 1;
    }
    /* Every audio track the engine knows has to be in the script's table too, or
       the extractor would quietly leave one theme unnamed. */
    ASSERT_EQ_INT(CDTRACKS_LAST_AUDIO - CDTRACKS_FIRST_AUDIO + 1, seen);
}

/* Pull a `U8 <name>[] = { ... };` initialiser out of MUSIC.CPP as (value, hasJingleFlag)
   pairs. Entries look like "JINGLE | 9, // comment" or "3, // comment". */
static bool read_track_table(const char *table, std::vector<std::pair<int, bool>> &out) {
    std::string path = std::string(TEST_SOURCE_ROOT) + "SOURCES/MUSIC.CPP";
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        text.append(buf, n);
    fclose(f);

    size_t start = text.find(std::string("U8 ") + table + "[]");
    if (start == std::string::npos)
        return false;
    start = text.find('{', start);
    size_t end = text.find("};", start);
    if (start == std::string::npos || end == std::string::npos)
        return false;

    std::string body = text.substr(start + 1, end - start - 1);
    size_t i = 0;
    while (i < body.size()) {
        size_t comma = body.find(',', i);
        if (comma == std::string::npos)
            break;
        std::string item = body.substr(i, comma - i);
        size_t slash = item.find("//"); /* strip a trailing comment */
        if (slash != std::string::npos)
            item = item.substr(0, slash);
        bool jingle = item.find("JINGLE") != std::string::npos;
        size_t d = item.find_first_of("0123456789");
        if (d != std::string::npos)
            out.push_back(std::make_pair(atoi(item.c_str() + d), jingle));
        i = comma + 1;
    }
    return !out.empty();
}

/* PlayMusic routes both tables through PlayJingle, which is only the same music
   either way because TrackCD and TrackCDUS hold identical music numbers and
   differ solely in the JINGLE flag: the flag says "this one is CD audio on the
   original medium", not "this one is a different piece of music". If that ever
   stops being true, the routing silently plays the wrong track, so pin it. */
static void test_track_tables_agree_on_music_numbers(void) {
    std::vector<std::pair<int, bool>> eu, us;
    ASSERT_TRUE(read_track_table("TrackCD", eu));
    ASSERT_TRUE(read_track_table("TrackCDUS", us));
    ASSERT_EQ_INT((int)eu.size(), (int)us.size());
    if (eu.size() != us.size())
        return;

    bool sameNumbers = true, euAllJingle = true, usHasCdEntries = false;
    for (size_t i = 0; i < eu.size(); i++) {
        if (eu[i].first != us[i].first)
            sameNumbers = false;
        if (!eu[i].second)
            euAllJingle = false;
        if (!us[i].second)
            usHasCdEntries = true;
    }
    ASSERT_TRUE(sameNumbers);    /* same music, whichever release */
    ASSERT_TRUE(euAllJingle);    /* the EU disc carries no CD audio at all */
    ASSERT_TRUE(usHasCdEntries); /* the US disc does, or the fork is pointless */
}

int main(void) {
    RUN_TEST(test_names_by_number);
    RUN_TEST(test_number_by_name);
    RUN_TEST(test_round_trip);
    RUN_TEST(test_agrees_with_music_cpp);
    RUN_TEST(test_agrees_with_extractor);
    RUN_TEST(test_track_tables_agree_on_music_numbers);
    TEST_SUMMARY();
    return test_failures != 0;
}
