/* The XAudio2 build's WAV decoder (WAV_DECODE.CPP) against SDL's loader, which
   the SDL builds use for the same music: same success, spec and bytes, with no
   WAV hints set (the XAudio2 build sets none). Generated files cover PCM and
   IMA ADPCM geometry; WAV files named on the command line are compared too,
   for checking real game music by hand:
     test_wav_decode JADPCM02.WAV LOGADPCM.WAV */
#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/AUDIO_DEVICE.H"
#include "../../LIB386/AIL/SDL/WAV_DECODE.H"
#include <SYSTEM/CRASH.H>
#include <SYSTEM/LE.H>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

void Crash_PrepareThread(void) {}

static U32 randomState = 0x5EED1234;
static U32 random_value() {
    randomState = randomState * 1103515245u + 12345u;
    return randomState >> 16;
}
static void put16(std::vector<U8> &v, U32 n) {
    v.push_back((U8)n);
    v.push_back((U8)(n >> 8));
}
static void put32(std::vector<U8> &v, U32 n) {
    put16(v, n);
    put16(v, n >> 16);
}
static void chunk(std::vector<U8> &v, const char *name, const std::vector<U8> &data) {
    v.insert(v.end(), name, name + 4);
    put32(v, (U32)data.size());
    v.insert(v.end(), data.begin(), data.end());
    if (data.size() & 1)
        v.push_back(0);
}

/* Same success, spec and PCM from both loaders. */
static void compare(const std::vector<U8> &wav, const char *what) {
    AILAudioSpec expectedSpec, spec;
    AIL_Zero(expectedSpec);
    AIL_Zero(spec);
    U8 *expected = NULL, *actual = NULL;
    U32 expectedBytes = 0, actualBytes = 0;
    const bool sdl = AILAudio_LoadWAVMemory(&wav[0], (int)wav.size(), &expectedSpec, &expected, &expectedBytes);
    const bool ours = AILWav_DecodeMemory(&wav[0], (int)wav.size(), &spec, &actual, &actualBytes);
    if (sdl != ours)
        printf("# %s: SDL %s, decoder %s\n", what, sdl ? "loads" : "refuses", ours ? "loads" : "refuses");
    ASSERT_EQ_INT(sdl, ours);
    if (sdl && ours) {
        ASSERT_EQ_INT(expectedSpec.format, spec.format);
        ASSERT_EQ_INT(expectedSpec.channels, spec.channels);
        ASSERT_EQ_INT(expectedSpec.freq, spec.freq);
        if (expectedBytes != actualBytes)
            printf("# %s: SDL %u bytes, decoder %u\n", what, (unsigned)expectedBytes, (unsigned)actualBytes);
        ASSERT_EQ_UINT(expectedBytes, actualBytes);
        if (expectedBytes == actualBytes && expectedBytes) {
            const int differs = memcmp(expected, actual, expectedBytes);
            if (differs) {
                U32 at = 0;
                while (expected[at] == actual[at])
                    ++at;
                printf("# %s: first difference at byte %u of %u\n", what, (unsigned)at, (unsigned)expectedBytes);
            }
            ASSERT_EQ_INT(0, differs);
        }
    }
    AIL_Free(expected);
    free(actual);
}

struct Ima {
    U32 channels, blockBytes, blockFrames, blocks, tail;
    bool fact, extensible, list;
};

static std::vector<U8> ima_wav(const Ima &g) {
    std::vector<U8> format;
    put16(format, g.extensible ? 0xFFFE : 0x11);
    put16(format, g.channels);
    put32(format, 22050);
    put32(format, 11100 * g.channels);
    put16(format, g.blockBytes);
    put16(format, 4);
    if (g.extensible) {
        put16(format, 22);
        put16(format, g.blockFrames);
        put32(format, g.channels == 2 ? 3 : 4);
        const U8 guid[16] = {0x11, 0, 0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xAA, 0, 0x38, 0x9B, 0x71};
        format.insert(format.end(), guid, guid + 16);
    } else {
        put16(format, 2);
        put16(format, g.blockFrames);
    }
    std::vector<U8> data;
    for (U32 b = 0; b < g.blocks; ++b) {
        for (U32 c = 0; c < g.channels; ++c) {
            put16(data, random_value());
            data.push_back((U8)random_value()); // any step index byte, out of range included
            data.push_back(0);
        }
        while (data.size() % g.blockBytes)
            data.push_back((U8)random_value());
    }
    for (U32 i = 0; i < g.tail; ++i)
        data.push_back((U8)random_value());
    std::vector<U8> wav = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
    chunk(wav, "fmt ", format);
    if (g.fact) {
        std::vector<U8> f;
        put32(f, g.blocks * 7 + 3);
        chunk(wav, "fact", f);
    }
    if (g.list) {
        std::vector<U8> info(7, 'x');
        chunk(wav, "LIST", info);
    }
    chunk(wav, "data", data);
    WriteLE_U32(&wav[4], (U32)wav.size() - 8);
    return wav;
}

static void test_ima_adpcm_matches_sdl() {
    const U32 sizes[] = {8, 12, 16, 20, 36, 40, 256, 512, 1024};
    int cases = 0;
    for (U32 channels = 1; channels <= 2; ++channels)
        for (U32 blockBytes : sizes) {
            if (blockBytes < channels * 4)
                continue;
            const U32 most = (blockBytes - channels * 4) * 2 / channels + 1;
            const U32 framesChoices[] = {0, most, most > 2 ? most - 1 : most, most > 9 ? most - 9 : most};
            for (U32 blockFrames : framesChoices)
                for (U32 blocks : {0u, 1u, 3u})
                    for (U32 tail : {0u, 3u, blockBytes - 1}) {
                        Ima g = {channels, blockBytes, blockFrames, blocks, tail, false, false, false};
                        g.fact = (cases % 3) == 0;
                        g.list = (cases % 4) == 1;
                        g.extensible = (cases % 5) == 2;
                        char what[128];
                        snprintf(what, sizeof(what), "IMA ch=%u block=%u frames=%u blocks=%u tail=%u%s",
                                 channels, blockBytes, blockFrames, blocks, tail, g.extensible ? " ext" : "");
                        compare(ima_wav(g), what);
                        ++cases;
                    }
        }
    ASSERT_TRUE(cases > 200);
}

/* The game's own shape: stereo, 22050 Hz, 1024-byte blocks, a fact chunk. */
static void test_ima_adpcm_like_the_cd_music() {
    Ima g = {2, 1024, 1017, 40, 300, true, false, false};
    compare(ima_wav(g), "CD-like stereo IMA ADPCM");
    Ima mono = {1, 512, 1017, 40, 0, true, false, false};
    compare(ima_wav(mono), "mono IMA ADPCM");
}

static void test_ima_adpcm_refusals_match_sdl() {
    Ima g = {2, 1024, 1017, 2, 0, false, false, false};
    std::vector<U8> wav = ima_wav(g);
    std::vector<U8> bits3 = wav;
    bits3[12 + 8 + 14] = 3;
    compare(bits3, "3-bit IMA ADPCM");
    std::vector<U8> odd = wav;
    odd[12 + 8 + 12] = 0xFE; // a block size that is not a multiple of 4
    compare(odd, "odd block size");
    std::vector<U8> tooMany = wav;
    tooMany[12 + 8 + 18] = 0xFF; // more samples per block than fit
    tooMany[12 + 8 + 19] = 0x03;
    compare(tooMany, "too many samples per block");
}

static void test_pcm_matches_sdl() {
    for (U32 bits : {8u, 16u})
        for (U32 channels = 1; channels <= 2; ++channels)
            for (U32 tail : {0u, 1u}) {
                std::vector<U8> format;
                put16(format, 1);
                put16(format, channels);
                put32(format, 11025);
                put32(format, 11025 * channels * bits / 8);
                put16(format, channels * bits / 8);
                put16(format, bits);
                std::vector<U8> data;
                for (U32 i = 0; i < 1000 * channels * bits / 8 + tail; ++i)
                    data.push_back((U8)random_value());
                std::vector<U8> wav = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
                chunk(wav, "fmt ", format);
                chunk(wav, "data", data);
                WriteLE_U32(&wav[4], (U32)wav.size() - 8);
                char what[64];
                snprintf(what, sizeof(what), "PCM %u-bit ch=%u tail=%u", bits, channels, tail);
                compare(wav, what);
            }
}

/* Where the chunks sit and what the RIFF size says: SDL reads chunks only up to
   the RIFF size (any size when it is zero), stops once it has fmt and data,
   and refuses fmt after data. */
static void test_chunk_layouts_match_sdl() {
    std::vector<U8> format;
    put16(format, 1);
    put16(format, 1);
    put32(format, 11025);
    put32(format, 22050);
    put16(format, 2);
    put16(format, 16);
    std::vector<U8> data, other, junk(10, 'j');
    for (int i = 0; i < 200; ++i) {
        data.push_back((U8)random_value());
        other.push_back((U8)random_value());
    }
    struct Layout {
        const char *what;
        const char *chunks; // f fmt, d data, o a second data chunk, j junk
        int riffSize;       // -1: the whole file; -2: up to the end of the second chunk
    };
    const Layout layouts[] = {
        {"data before fmt", "df", -1},
        {"data only", "d", -1},
        {"two data chunks", "fdo", -1},
        {"a second fmt after data", "fdf", -1},
        {"RIFF size zero", "fd", 0},
        {"RIFF size ends before data", "fjd", -2},
        {"RIFF size ends inside data", "fd", 40},
        {"junk then data past the RIFF size", "fjjd", -2},
    };
    for (const Layout &l : layouts) {
        std::vector<U8> wav = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
        U32 secondEnd = 0;
        for (const char *c = l.chunks; *c; ++c) {
            if (*c == 'f')
                chunk(wav, "fmt ", format);
            else if (*c == 'd')
                chunk(wav, "data", data);
            else if (*c == 'o')
                chunk(wav, "data", other);
            else
                chunk(wav, "junk", junk);
            if (c - l.chunks == 1)
                secondEnd = (U32)wav.size();
        }
        const U32 size = l.riffSize == -1 ? (U32)wav.size() - 8 : l.riffSize == -2 ? secondEnd - 8
                                                                                   : (U32)l.riffSize;
        WriteLE_U32(&wav[4], size);
        compare(wav, l.what);
    }
}

static std::vector<const char *> files;

static void test_named_files_match_sdl() {
    for (const char *name : files) {
        FILE *file = fopen(name, "rb");
        ASSERT_TRUE(file != NULL);
        if (!file)
            continue;
        std::vector<U8> wav;
        U8 buffer[65536];
        size_t got;
        while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
            wav.insert(wav.end(), buffer, buffer + got);
        fclose(file);
        ASSERT_TRUE(wav.size() > 12);
        if (wav.size() > 12)
            compare(wav, name);
        printf("# compared %s (%u bytes)\n", name, (unsigned)wav.size());
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i)
        files.push_back(argv[i]);
    RUN_TEST(test_ima_adpcm_matches_sdl);
    RUN_TEST(test_ima_adpcm_like_the_cd_music);
    RUN_TEST(test_ima_adpcm_refusals_match_sdl);
    RUN_TEST(test_pcm_matches_sdl);
    RUN_TEST(test_chunk_layouts_match_sdl);
    if (!files.empty())
        RUN_TEST(test_named_files_match_sdl);
    TEST_SUMMARY();
    return test_failures != 0;
}
