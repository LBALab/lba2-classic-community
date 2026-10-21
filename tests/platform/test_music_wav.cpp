#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/WAV_STREAM.H"
#include <SYSTEM/CRASH.H>
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LE.H>
#include <algorithm>
#include <cstring>
#include <vector>

void Crash_PrepareThread(void) {}

static char path[] = TEST_TMP_DIR "/music-stream.wav";
static U32 randomState = 0xDEADBEEF;
static U32 random_value() {
    randomState = randomState * 1103515245u + 12345u;
    return randomState >> 16;
}
static void put16(std::vector<Uint8> &v, U32 n) {
    v.push_back((Uint8)n);
    v.push_back((Uint8)(n >> 8));
}
static void put32(std::vector<Uint8> &v, U32 n) {
    put16(v, n);
    put16(v, n >> 16);
}
static void chunk(std::vector<Uint8> &v, const char *name, const std::vector<Uint8> &data) {
    v.insert(v.end(), name, name + 4);
    put32(v, (U32)data.size());
    v.insert(v.end(), data.begin(), data.end());
    if (data.size() & 1)
        v.push_back(0);
}
static std::vector<Uint8> fixture(U32 code, U32 channels, U32 blocks, bool fact = false, U32 bits = 0) {
    bool adpcm = code == 2 || code == 17;
    if (!bits)
        bits = adpcm ? 4 : code == 3            ? 32
                       : code == 6 || code == 7 ? 8
                                                : 16;
    U32 align = adpcm ? channels * 64 : channels * bits / 8;
    U32 frames = adpcm ? (align - channels * (code == 2 ? 7 : 4)) * 2 / channels +
                             (code == 2 ? 2 : 1)
                       : 1;
    std::vector<Uint8> format;
    put16(format, code);
    put16(format, channels);
    put32(format, 22050);
    put32(format, 22050 * align / frames);
    put16(format, align);
    put16(format, bits);
    if (adpcm) {
        put16(format, code == 2 ? 32 : 2);
        put16(format, frames);
        if (code == 2) {
            put16(format, 7);
            const int coefficients[] = {256, 0, 512, -256, 0, 0, 192, 64, 240, 0, 460, -208, 392, -232};
            for (int n : coefficients)
                put16(format, (U16)n);
        }
    }
    std::vector<Uint8> data;
    for (U32 b = 0; b < blocks; ++b) {
        size_t start = data.size();
        if (code == 2) {
            for (U32 ch = 0; ch < channels; ++ch)
                data.push_back((Uint8)(b % 7));
            for (U32 ch = 0; ch < channels; ++ch)
                put16(data, 16 + b % 100);
            for (int sample = 0; sample < 2; ++sample)
                for (U32 ch = 0; ch < channels; ++ch)
                    put16(data, random_value());
        } else if (code == 17) {
            for (U32 ch = 0; ch < channels; ++ch) {
                put16(data, random_value());
                data.push_back((Uint8)(b % 89));
                data.push_back(0);
            }
        }
        while (data.size() < start + align)
            data.push_back((Uint8)random_value());
    }
    std::vector<Uint8> wav = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
    chunk(wav, "fmt ", format);
    if (fact) {
        std::vector<Uint8> f;
        put32(f, blocks * frames > 5 ? blocks * frames - 5 : blocks * frames);
        chunk(wav, "fact", f);
    }
    chunk(wav, "data", data);
    WriteLE_U32(&wav[4], (U32)wav.size() - 8);
    return wav;
}
static void write_fixture(const std::vector<Uint8> &wav, const char *target = path) {
    FILE *file = fopen(target, "wb");
    ASSERT_TRUE(file != NULL);
    if (!file)
        return;
    ASSERT_EQ_UINT(wav.size(), fwrite(&wav[0], 1, wav.size(), file));
    ASSERT_EQ_INT(0, fclose(file));
}
static void compare(const std::vector<Uint8> &wav, S32 skip = 0, bool image = false) {
    write_fixture(wav);
    SDL_AudioSpec expectedSpec;
    Uint8 *expected = NULL;
    Uint32 expectedBytes = 0;
    ASSERT_TRUE(AILAudio_LoadWAVMemory(&wav[0], (int)wav.size(), &expectedSpec, &expected, &expectedBytes));
    if (!expected)
        return;
    bool recognized = false;
    AILWavSource *source = AILWav_Open(path, image, skip, &recognized);
    ASSERT_TRUE(recognized && source);
    if (!source) {
        SDL_free(expected);
        return;
    }
    if (image) {
        S32 asset = DiscImage_OpenRead(TEST_TMP_DIR "/LBA2.HQR");
        ASSERT_TRUE(asset > 0);
        Uint8 marker = 0;
        ASSERT_EQ_INT(1, DiscImage_Read(asset, &marker, 1));
        ASSERT_EQ_INT(0x42, marker);
        DiscImage_Close(asset);
        DiscImage_Unmount();
    }
    ASSERT_TRUE(AILWav_TestBytesRead(source) <= 32768);
    std::vector<Uint8> actual;
    for (;;) {
        SDL_AudioSpec spec;
        Uint8 *pcm = NULL;
        Uint32 bytes = 0;
        int result = AILWav_Read(source, &spec, &pcm, &bytes);
        ASSERT_TRUE(result >= 0);
        if (result <= 0)
            break;
        ASSERT_EQ_INT(expectedSpec.freq, spec.freq);
        ASSERT_EQ_INT(expectedSpec.channels, spec.channels);
        ASSERT_EQ_INT(expectedSpec.format, spec.format);
        ASSERT_TRUE(bytes <= 262144);
        if (bytes)
            actual.insert(actual.end(), pcm, pcm + bytes);
        SDL_free(pcm);
    }
    size_t offset = std::min((size_t)expectedBytes,
                             (size_t)skip * (AIL_AUDIO_BITSIZE(expectedSpec.format) / 8) * expectedSpec.channels);
    ASSERT_EQ_UINT(expectedBytes - offset, actual.size());
    if (!actual.empty() && actual.size() == expectedBytes - offset)
        ASSERT_MEM_EQ(expected + offset, &actual[0], actual.size());
    AILWav_Close(source);
    SDL_free(expected);
}
static void test_exact_pcm_and_adpcm() {
    for (U32 code : {1u, 2u, 3u, 6u, 7u, 17u})
        for (U32 channels : {1u, 2u})
            for (U32 blocks : {1u, 3u, 73u})
                compare(fixture(code, channels, blocks));
}
static U32 find_chunk(const std::vector<Uint8> &wav, const char *name) {
    for (U32 p = 12; p + 8 <= wav.size();) {
        if (!memcmp(&wav[p], name, 4))
            return p;
        U32 bytes = ReadLE_U32(&wav[p + 4]);
        p += 8 + bytes + (bytes & 1);
    }
    return 0;
}
static void test_pcm_depths_and_extensible() {
    for (U32 bits : {8u, 16u, 24u, 32u}) {
        std::vector<Uint8> wav = fixture(1, 2, 7001, false, bits);
        compare(wav, 2123);
        wav.insert(wav.begin() + 36, 24, 0);
        WriteLE_U32(&wav[4], (U32)wav.size() - 8);
        WriteLE_U32(&wav[16], 40);
        WriteLE_U16(&wav[20], 0xFFFE);
        WriteLE_U16(&wav[36], 22);
        WriteLE_U16(&wav[38], (U16)bits);
        WriteLE_U32(&wav[44], 1);
        const Uint8 guid[] = {0, 0, 0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71};
        memcpy(&wav[48], guid, sizeof(guid));
        compare(wav, 2123);
    }
}
static void test_partial_blocks() {
    for (const char *hint : {"dropframe", "dropblock"}) {
        SDL_SetHint(SDL_HINT_WAVE_TRUNCATION, hint);
        for (U32 code : {2u, 17u})
            for (U32 channels : {1u, 2u})
                for (U32 cut : {1u, 3u, 31u}) {
                    std::vector<Uint8> wav = fixture(code, channels, 73);
                    U32 data = find_chunk(wav, "data");
                    WriteLE_U32(&wav[data + 4], ReadLE_U32(&wav[data + 4]) - cut);
                    wav.resize(wav.size() - cut);
                    if (wav.size() & 1)
                        wav.push_back(0);
                    WriteLE_U32(&wav[4], (U32)wav.size() - 8);
                    compare(wav);
                }
    }
    SDL_SetHint(SDL_HINT_WAVE_TRUNCATION, "");
}
static void reject_riff(const std::vector<Uint8> &wav) {
    SDL_AudioSpec spec;
    Uint8 *pcm = NULL;
    Uint32 bytes = 0;
    ASSERT_TRUE(!AILAudio_LoadWAVMemory(&wav[0], (int)wav.size(), &spec, &pcm, &bytes));
    SDL_free(pcm);
    write_fixture(wav);
    bool recognized = false;
    AILWavSource *source = AILWav_Open(path, false, 0, &recognized);
    ASSERT_TRUE(recognized && source == NULL);
    if (source)
        AILWav_Close(source);
}
static void test_riff_size_policies() {
    for (U32 code : {1u, 2u, 17u}) {
        for (const char *hint : {"", "ignorezero", "ignore", "maximum", "unknown"}) {
            SDL_SetHint(SDL_HINT_WAVE_RIFF_CHUNK_SIZE, hint);
            std::vector<Uint8> wav = fixture(code, 2, 73);
            WriteLE_U32(&wav[4], 0);
            compare(wav, 17);
        }
        for (const char *hint : {"ignore", "maximum"}) {
            SDL_SetHint(SDL_HINT_WAVE_RIFF_CHUNK_SIZE, hint);
            for (U32 size : {1u, 4u, 32u, 0xFFFFFFFFu}) {
                std::vector<Uint8> wav = fixture(code, 2, 73);
                WriteLE_U32(&wav[4], size);
                compare(wav);
            }
        }
        for (const char *hint : {"", "force", "ignorezero"}) {
            SDL_SetHint(SDL_HINT_WAVE_RIFF_CHUNK_SIZE, hint);
            std::vector<Uint8> wav = fixture(code, 2, 73);
            U32 data = find_chunk(wav, "data");
            /* RIFF bounds the chunk search, not a chunk's payload. */
            for (U32 size : {data - 7, (U32)wav.size() + 123, 0xFFFFFFFFu}) {
                WriteLE_U32(&wav[4], size);
                compare(wav);
            }
            WriteLE_U32(&wav[4], data - 8);
            reject_riff(wav);
            WriteLE_U32(&wav[4], 4);
            reject_riff(wav);
        }
        SDL_SetHint(SDL_HINT_WAVE_RIFF_CHUNK_SIZE, "force");
        std::vector<Uint8> wav = fixture(code, 2, 73);
        WriteLE_U32(&wav[4], 0);
        reject_riff(wav);
    }
    SDL_SetHint(SDL_HINT_WAVE_RIFF_CHUNK_SIZE, "");
}
static void test_riff_strict_truncation() {
    SDL_SetHint(SDL_HINT_WAVE_TRUNCATION, "verystrict");
    for (const char *hint : {"force", "ignorezero"}) {
        SDL_SetHint(SDL_HINT_WAVE_RIFF_CHUNK_SIZE, hint);
        std::vector<Uint8> wav = fixture(1, 2, 73);
        compare(wav);
        WriteLE_U32(&wav[4], (U32)wav.size() - 9);
        reject_riff(wav);
        WriteLE_U32(&wav[4], 0xFFFFFFFFu);
        reject_riff(wav);
    }
    for (const char *hint : {"", "ignorezero", "ignore", "maximum"}) {
        SDL_SetHint(SDL_HINT_WAVE_RIFF_CHUNK_SIZE, hint);
        std::vector<Uint8> wav = fixture(1, 2, 73);
        WriteLE_U32(&wav[4], 0);
        reject_riff(wav);
    }
    SDL_SetHint(SDL_HINT_WAVE_RIFF_CHUNK_SIZE, "");
    SDL_SetHint(SDL_HINT_WAVE_TRUNCATION, "");
}
static void test_seek_and_fact_trimming() {
    for (U32 code : {1u, 2u, 17u}) {
        for (const char *hint : {"ignore", "truncate", "ignorezero", "strict"}) {
            SDL_SetHint(SDL_HINT_WAVE_FACT_CHUNK, hint);
            for (S32 skip : {0, 1, 17, 120})
                compare(fixture(code, 2, 90, true), skip);
        }
    }
    SDL_SetHint(SDL_HINT_WAVE_FACT_CHUNK, "");
}
static void test_bounded_first_read() {
    std::vector<Uint8> wav = fixture(2, 2, 20000);
    write_fixture(wav);
    bool recognized;
    AILWavSource *source = AILWav_Open(path, false, 0, &recognized);
    ASSERT_TRUE(source && recognized);
    if (!source)
        return;
    SDL_AudioSpec spec;
    Uint8 *pcm;
    Uint32 bytes;
    ASSERT_EQ_INT(1, AILWav_Read(source, &spec, &pcm, &bytes));
    ASSERT_TRUE(AILWav_TestBytesRead(source) <= 32768);
    ASSERT_TRUE(bytes <= 16384);
    SDL_free(pcm);
    AILWav_Close(source);
}
static U32 iso_record(Uint8 *p, U32 lba, U32 size, bool directory, const char *name) {
    U32 n = name ? (U32)strlen(name) : 1;
    U32 length = (33 + n + 1) & ~1u;
    p[0] = (Uint8)length;
    for (U32 offset : {2u, 10u}) {
        U32 value = offset == 2 ? lba : size;
        WriteLE_U32(p + offset, value);
        for (U32 i = 0; i < 4; ++i)
            p[offset + 4 + i] = (Uint8)(value >> (24 - 8 * i));
    }
    p[25] = directory ? 2 : 0;
    p[28] = p[31] = 1;
    p[32] = (Uint8)n;
    if (name)
        memcpy(p + 33, name, n);
    return length;
}
static void test_image_wav_survives_unmount() {
    std::vector<Uint8> wav = fixture(2, 2, 3000);
    std::vector<Uint8> image((20 + (wav.size() + 2047) / 2048) * 2048, 0);
    Uint8 *pvd = &image[16 * 2048];
    pvd[0] = pvd[6] = 1;
    memcpy(pvd + 1, "CD001", 5);
    iso_record(pvd + 156, 18, 2048, true, NULL);
    Uint8 *root = &image[18 * 2048];
    U32 offset = iso_record(root, 19, 1, false, "LBA2.HQR;1");
    iso_record(root + offset, 20, (U32)wav.size(), false, "MUSIC-STREAM.WAV;1");
    image[19 * 2048] = 0x42;
    memcpy(&image[20 * 2048], &wav[0], wav.size());
    const char *imagePath = TEST_TMP_DIR "/music-stream.iso";
    write_fixture(image, imagePath);
    DiscImage_SetOverride(imagePath);
    ASSERT_TRUE(DiscImage_Mount(TEST_TMP_DIR "/", "lba2.hqr"));
    compare(wav, 17, true);
    DiscImage_Unmount();
    DiscImage_SetOverride(NULL);
    ASSERT_EQ_INT(0, remove(imagePath));
}
static void test_randomized() {
    randomState = 0xDEADBEEF;
    for (int round = 0; round < 30; ++round) {
        U32 code = round % 2 ? 2 : 17;
        U32 channels = 1 + random_value() % 2;
        U32 blocks = 3 + random_value() % 120;
        std::vector<Uint8> wav = fixture(code, channels, blocks);
        compare(wav, (S32)(random_value() % 64));
    }
}
static void test_invalid_input() {
    std::vector<Uint8> wav = fixture(2, 2, 3);
    wav.pop_back();
    write_fixture(wav);
    bool recognized;
    AILWavSource *source = AILWav_Open(path, false, 0, &recognized);
    ASSERT_TRUE(recognized && source == NULL);
    if (source)
        AILWav_Close(source);
    wav = fixture(2, 2, 3);
    WriteLE_U16(&wav[32], 0);
    write_fixture(wav);
    source = AILWav_Open(path, false, 0, &recognized);
    ASSERT_TRUE(recognized && source == NULL);
    if (source)
        AILWav_Close(source);
    wav = fixture(2, 2, 73, true);
    WriteLE_U32(&wav[find_chunk(wav, "fact") + 8], 0x100000);
    write_fixture(wav);
    SDL_SetHint(SDL_HINT_WAVE_FACT_CHUNK, "strict");
    SDL_AudioSpec spec;
    Uint8 *pcm = NULL;
    Uint32 bytes = 0;
    ASSERT_TRUE(!AILAudio_LoadWAVMemory(&wav[0], (int)wav.size(), &spec, &pcm, &bytes));
    SDL_free(pcm);
    source = AILWav_Open(path, false, 0, &recognized);
    ASSERT_TRUE(source != NULL);
    if (source) {
        int result;
        do {
            result = AILWav_Read(source, &spec, &pcm, &bytes);
            SDL_free(pcm);
        } while (result > 0);
        ASSERT_EQ_INT(-1, result);
        AILWav_Close(source);
    }
    SDL_SetHint(SDL_HINT_WAVE_FACT_CHUNK, "");
}
int main() {
#ifdef LBA2_USE_SDL2
    SDL_SetMainReady();
#endif
    RUN_TEST(test_exact_pcm_and_adpcm);
    RUN_TEST(test_seek_and_fact_trimming);
    RUN_TEST(test_pcm_depths_and_extensible);
    RUN_TEST(test_partial_blocks);
    RUN_TEST(test_riff_size_policies);
    RUN_TEST(test_riff_strict_truncation);
    RUN_TEST(test_bounded_first_read);
    RUN_TEST(test_image_wav_survives_unmount);
    RUN_TEST(test_randomized);
    RUN_TEST(test_invalid_input);
    remove(path);
    TEST_SUMMARY();
    return test_failures != 0;
}
