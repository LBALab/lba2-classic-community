/* Host regression test for the CUE-sheet reader (LIB386/SYSTEM/CUE.CPP).
 * Pins Cue_ParseExternalAudio: it returns the first external (non-BINARY) AUDIO
 * track's file (GOG's LBA2.OGG = music track 6), skips a data-only cue, and
 * skips an AUDIO track that sits under a BINARY file (in-image CD-DA, a separate
 * source). The parser is pure text, so the test compiles it directly: no SDL,
 * no filesystem, no disc image. */
#include "test_harness.h"

#include <SYSTEM/CUE.H>

#include <string.h>

/* The GOG LBA2 cue: a BINARY data track plus one standalone OGG audio track. */
static const char *const GOG_CUE =
    "FILE \"LBA2.GOG\" BINARY\n"
    "  TRACK 01 MODE1/2352\n"
    "    INDEX 01 00:00:00\n"
    "FILE \"LBA2.OGG\" MP3\n"
    "  TRACK 02 AUDIO\n"
    "      INDEX 01 00:00:00\n";

/* A pure data disc: no audio track at all. */
static const char *const DATA_ONLY_CUE =
    "FILE \"GAME.BIN\" BINARY\n"
    "  TRACK 01 MODE1/2352\n"
    "    INDEX 01 00:00:00\n";

/* A single-BIN rip whose audio tracks live inside the BINARY image (located by
   INDEX). That is in-image CD-DA, not an external file, so it must be skipped. */
static const char *const INBIN_CDDA_CUE =
    "FILE \"disc.bin\" BINARY\n"
    "  TRACK 01 MODE1/2352\n"
    "    INDEX 01 00:00:00\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 01 04:32:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 01 09:10:33\n";

/* Two external audio files: the first is reported. */
static const char *const MULTI_AUDIO_CUE =
    "FILE \"data.bin\" BINARY\n"
    "  TRACK 01 MODE1/2352\n"
    "FILE \"track2.wav\" WAVE\n"
    "  TRACK 02 AUDIO\n"
    "FILE \"track3.wav\" WAVE\n"
    "  TRACK 03 AUDIO\n";

static void test_gog_external_ogg(void) {
    char name[64] = "";
    ASSERT_EQ_INT(1, Cue_ParseExternalAudio(GOG_CUE, strlen(GOG_CUE), name, sizeof(name)));
    ASSERT_TRUE(strcmp(name, "LBA2.OGG") == 0);
}

static void test_data_only_has_no_audio(void) {
    char name[64] = "x";
    ASSERT_EQ_INT(0, Cue_ParseExternalAudio(DATA_ONLY_CUE, strlen(DATA_ONLY_CUE), name, sizeof(name)));
}

static void test_inbin_cdda_skipped(void) {
    /* AUDIO tracks under a BINARY file are in-image CD-DA, not an external file. */
    char name[64] = "x";
    ASSERT_EQ_INT(0, Cue_ParseExternalAudio(INBIN_CDDA_CUE, strlen(INBIN_CDDA_CUE), name, sizeof(name)));
}

static void test_first_external_audio_wins(void) {
    char name[64] = "";
    ASSERT_EQ_INT(1, Cue_ParseExternalAudio(MULTI_AUDIO_CUE, strlen(MULTI_AUDIO_CUE), name, sizeof(name)));
    ASSERT_TRUE(strcmp(name, "track2.wav") == 0);
}

int main(void) {
    RUN_TEST(test_gog_external_ogg);
    RUN_TEST(test_data_only_has_no_audio);
    RUN_TEST(test_inbin_cdda_skipped);
    RUN_TEST(test_first_external_audio_wins);
    TEST_SUMMARY();
    return test_failures != 0;
}
