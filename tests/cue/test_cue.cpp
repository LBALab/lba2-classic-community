/* Host regression test for the CUE-sheet reader (LIB386/SYSTEM/CUE.CPP).
 * Pins Cue_ParseExternalAudio: it returns the first external (non-BINARY) AUDIO
 * track's file (GOG's LBA2.OGG = music track 6), skips a data-only cue, and
 * skips an AUDIO track that sits under a BINARY file (in-image CD-DA, a separate
 * source). The parser is pure text, so the test compiles it directly: no SDL,
 * no filesystem, no disc image. */
#include "test_harness.h"

#include <SYSTEM/CUE.H>

#include <stdio.h>
#include <string.h>
#include <string>

#ifndef TEST_TMP_DIR
#define TEST_TMP_DIR "."
#endif

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

/* A rip whose audio tracks live in the image, with a hole in the numbering (no
   track 2). Holes happen when a rip loses a track, and the engine picks its
   music by CD track number, so the survivors have to keep their true numbers:
   renumbering to close the gap would shift every theme by one. */
static const char *const RETAIL_TOC_CUE =
    "FILE \"TWINSEN.bin\" BINARY\n"
    "  TRACK 01 MODE1/2352\n"
    "    INDEX 01 00:00:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 46:03:00\n"
    "    INDEX 01 46:04:68\n"
    "  TRACK 04 AUDIO\n"
    "    INDEX 01 49:50:59\n";

static void test_toc_reports_every_track(void) {
    CueTrack toc[CUE_MAX_TRACKS];
    int n = Cue_ParseToc(RETAIL_TOC_CUE, strlen(RETAIL_TOC_CUE), toc, CUE_MAX_TRACKS);
    ASSERT_EQ_INT(3, n);

    ASSERT_EQ_INT(1, toc[0].number);
    ASSERT_EQ_INT(0, toc[0].isAudio);
    ASSERT_EQ_INT(1, toc[0].fileIsBinary);
    ASSERT_TRUE(strcmp(toc[0].file, "TWINSEN.bin") == 0);

    /* Track numbers come from the sheet, holes and all. */
    ASSERT_EQ_INT(3, toc[1].number);
    ASSERT_EQ_INT(1, toc[1].isAudio);
    ASSERT_EQ_INT(1, toc[1].fileIsBinary);
    /* INDEX 01, not the INDEX 00 pregap: 46:04:68 = (46*60+4)*75+68. */
    ASSERT_EQ_INT((46 * 60 + 4) * 75 + 68, (int)toc[1].startFrame);

    ASSERT_EQ_INT(4, toc[2].number);
    ASSERT_EQ_INT((49 * 60 + 50) * 75 + 59, (int)toc[2].startFrame);
}

static void test_toc_external_files(void) {
    CueTrack toc[CUE_MAX_TRACKS];
    int n = Cue_ParseToc(MULTI_AUDIO_CUE, strlen(MULTI_AUDIO_CUE), toc, CUE_MAX_TRACKS);
    ASSERT_EQ_INT(3, n);
    ASSERT_EQ_INT(1, toc[0].fileIsBinary);
    ASSERT_EQ_INT(0, toc[1].fileIsBinary);
    ASSERT_TRUE(strcmp(toc[1].file, "track2.wav") == 0);
    ASSERT_TRUE(strcmp(toc[2].file, "track3.wav") == 0);
    /* No INDEX line at all: start of file, which is what an external track means. */
    ASSERT_EQ_INT(0, (int)toc[1].startFrame);
}

static void test_toc_gog_shape(void) {
    CueTrack toc[CUE_MAX_TRACKS];
    int n = Cue_ParseToc(GOG_CUE, strlen(GOG_CUE), toc, CUE_MAX_TRACKS);
    ASSERT_EQ_INT(2, n);
    ASSERT_EQ_INT(2, toc[1].number);
    ASSERT_EQ_INT(1, toc[1].isAudio);
    ASSERT_EQ_INT(0, toc[1].fileIsBinary);
    ASSERT_TRUE(strcmp(toc[1].file, "LBA2.OGG") == 0);
}

/* A truncated sheet must not produce garbage tracks. */
static void test_toc_malformed_index(void) {
    static const char *const BAD =
        "FILE \"d.bin\" BINARY\n"
        "  TRACK 02 AUDIO\n"
        "    INDEX 01 not-a-time\n";
    CueTrack toc[CUE_MAX_TRACKS];
    ASSERT_EQ_INT(1, Cue_ParseToc(BAD, strlen(BAD), toc, CUE_MAX_TRACKS));
    ASSERT_EQ_INT(2, toc[0].number);
    ASSERT_EQ_INT(0, (int)toc[0].startFrame);
}

/* Tracks past the caller's capacity are dropped. Their INDEX lines must be
   dropped with them: landing on the last kept entry would silently move a real
   track's start sector, which is the kind of wrong that plays as music. */
static void test_toc_overflow_does_not_corrupt(void) {
    CueTrack toc[2];
    int n = Cue_ParseToc(RETAIL_TOC_CUE, strlen(RETAIL_TOC_CUE), toc, 2);
    ASSERT_EQ_INT(2, n);
    ASSERT_EQ_INT(1, toc[0].number);
    ASSERT_EQ_INT(3, toc[1].number);
    /* Track 4's INDEX must not have overwritten track 3's. */
    ASSERT_EQ_INT((46 * 60 + 4) * 75 + 68, (int)toc[1].startFrame);
}

/* --disc takes an image or the cue that names one, so the resolution has to work
   off a real file and has to say "not a cue" rather than guess. The image path is
   relative to the sheet, not to the working directory. */
static void write_text(const std::string &path, const char *text) {
    FILE *f = fopen(path.c_str(), "wb");
    if (f) {
        fwrite(text, 1, strlen(text), f);
        fclose(f);
    }
}

static void test_image_for_sheet(void) {
    std::string dir = std::string(TEST_TMP_DIR) + "/";
    std::string cue = dir + "cue_sheet_fixture.cue";
    write_text(cue, RETAIL_TOC_CUE);

    char out[512] = "";
    ASSERT_EQ_INT(1, Cue_ImageForSheet(cue.c_str(), out, sizeof(out)));
    ASSERT_TRUE(std::string(out) == dir + "TWINSEN.bin");

    /* A sheet with no BINARY track names no image. */
    std::string ext = dir + "cue_external_fixture.cue";
    write_text(ext, "FILE \"track02.wav\" WAVE\n  TRACK 02 AUDIO\n");
    ASSERT_EQ_INT(0, Cue_ImageForSheet(ext.c_str(), out, sizeof(out)));

    /* Not a cue at all: the caller passes the path through as an image. */
    std::string bin = dir + "cue_notacue_fixture.bin";
    write_text(bin, "\x00\x01\x02 this is not a cue sheet at all");
    ASSERT_EQ_INT(0, Cue_ImageForSheet(bin.c_str(), out, sizeof(out)));

    ASSERT_EQ_INT(0, Cue_ImageForSheet((dir + "does_not_exist.cue").c_str(), out, sizeof(out)));
    ASSERT_EQ_INT(0, Cue_ImageForSheet(NULL, out, sizeof(out)));

    remove(cue.c_str());
    remove(ext.c_str());
    remove(bin.c_str());
}

int main(void) {
    RUN_TEST(test_gog_external_ogg);
    RUN_TEST(test_data_only_has_no_audio);
    RUN_TEST(test_inbin_cdda_skipped);
    RUN_TEST(test_first_external_audio_wins);
    RUN_TEST(test_toc_reports_every_track);
    RUN_TEST(test_toc_external_files);
    RUN_TEST(test_toc_gog_shape);
    RUN_TEST(test_toc_malformed_index);
    RUN_TEST(test_toc_overflow_does_not_corrupt);
    RUN_TEST(test_image_for_sheet);
    TEST_SUMMARY();
    return test_failures != 0;
}
