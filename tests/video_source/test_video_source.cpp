#include "../test_harness.h"
#include "VIDEO_SOURCE.H"
#include <SYSTEM/FILES.H>
#include <SYSTEM/LE.H>
#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <vector>

static std::vector<U8> archiveBytes, movieBytes;
static size_t filePosition, bytesRead, largestRead;
static int openHandles, errorLogs;
static size_t maxRead = (size_t)-1, failAt = (size_t)-1;
static bool missingFile;

extern "C" S32 OpenRead(const char *) {
    if (missingFile)
        return 0;
    ++openHandles;
    filePosition = 0;
    return 1;
}
extern "C" S32 Close(S32) {
    --openHandles;
    return 0;
}
extern "C" S32 Read(S32, void *out, U32 bytes) {
    if (filePosition >= failAt)
        return -1;
    size_t n = std::min((size_t)bytes, archiveBytes.size() - filePosition);
    n = std::min(n, std::min(maxRead, failAt - filePosition));
    if (n)
        memcpy(out, &archiveBytes[filePosition], n);
    filePosition += n;
    bytesRead += n;
    largestRead = std::max(largestRead, n);
    return (S32)n;
}
extern "C" S32 Seek(S32, S32 offset, ESeekOrigin origin) {
    long long base = origin == SEEK_FROM_START ? 0 : origin == SEEK_FROM_END ? (long long)archiveBytes.size()
                                                                             : (long long)filePosition;
    long long pos = base + offset;
    if (pos < 0 || pos > (long long)archiveBytes.size())
        return -1;
    filePosition = (size_t)pos;
    return (S32)pos;
}
extern "C" void Log_Error(const char *, ...) { ++errorLogs; }

static void u32(std::vector<U8> &out, U32 value) {
    for (int i = 0; i < 4; ++i)
        out.push_back((U8)(value >> (8 * i)));
}
struct Bits {
    std::vector<U8> data;
    size_t position = 0;
    void put(U32 value, int count) {
        for (int i = 0; i < count; ++i, ++position) {
            if (position / 8 == data.size())
                data.push_back(0);
            data[position / 8] |= ((value >> i) & 1) << (position % 8);
        }
    }
    void tree(U32 value) {
        put(1, 1);
        for (int shift = 0; shift <= 8; shift += 8) {
            put(1, 1);
            put(0, 1);
            put(value >> shift, 8);
            put(0, 1);
        }
        put(0xFFFF, 16);
        put(0xFFFE, 16);
        put(0xFFFD, 16);
        put(0, 1);
        put(0, 1);
    }
};

static void fixture(unsigned frames, unsigned chunkBytes = 36) {
    Bits trees;
    for (int i = 0; i < 4; ++i)
        trees.tree(i == 3 ? 0x0103 : 0);
    movieBytes.clear();
    u32(movieBytes, 0x324B4D53); // SMK2
    u32(movieBytes, 320);
    u32(movieBytes, 200);
    u32(movieBytes, frames);
    u32(movieBytes, 67);
    u32(movieBytes, 0);
    for (int i = 0; i < 7; ++i)
        u32(movieBytes, i == 0 || i == 3 ? 8 : 0);
    u32(movieBytes, (U32)trees.data.size());
    for (int i = 0; i < 4; ++i)
        u32(movieBytes, 0);
    for (int i = 0; i < 7; ++i)
        u32(movieBytes, i == 0 || i == 3 ? 0x40000000 | 22050 : 0);
    u32(movieBytes, 0);
    for (unsigned i = 0; i < frames; ++i)
        u32(movieBytes, chunkBytes | 1);
    movieBytes.insert(movieBytes.end(), frames, 0x13);
    movieBytes.insert(movieBytes.end(), trees.data.begin(), trees.data.end());
    for (unsigned i = 0; i < frames; ++i) {
        size_t start = movieBytes.size();
        const U8 palette[] = {2, 0x80, (U8)(i % 64), (U8)((i * 3) % 64), (U8)((i * 7) % 64),
                              0xFF, 0xFD, 0};
        movieBytes.insert(movieBytes.end(), palette, palette + sizeof(palette));
        for (int track = 0; track < 2; ++track) {
            u32(movieBytes, 12);
            for (int sample = 0; sample < 8; ++sample)
                movieBytes.push_back((U8)(i * 17 + sample + track));
        }
        movieBytes.resize(start + chunkBytes, 0);
    }
    archiveBytes.clear();
    u32(archiveBytes, 8);
    u32(archiveBytes, (U32)movieBytes.size() + 18);
    u32(archiveBytes, (U32)movieBytes.size());
    u32(archiveBytes, (U32)movieBytes.size());
    archiveBytes.push_back(0);
    archiveBytes.push_back(0);
    archiveBytes.insert(archiveBytes.end(), movieBytes.begin(), movieBytes.end());
    filePosition = bytesRead = largestRead = 0;
    openHandles = errorLogs = 0;
    maxRead = failAt = (size_t)-1;
    missingFile = false;
}

static void compress(unsigned method, bool matches = true) {
    std::vector<U8> packed;
    size_t position = 0;
    while (position < movieBytes.size()) {
        size_t flagsAt = packed.size();
        packed.push_back(0);
        for (unsigned bit = 0; bit < 8 && position < movieBytes.size(); ++bit) {
            size_t length = 0, distance = 0;
            const unsigned distances[] = {1, 2, 3, 4, 8, 16, 31, 257, 4096};
            if (matches) {
                for (unsigned d : distances) {
                    if (d > position)
                        continue;
                    size_t n = 0;
                    while (n < method + 16 && position + n < movieBytes.size() &&
                           movieBytes[position + n] == movieBytes[position + n - d])
                        ++n;
                    if (n > length) {
                        length = n;
                        distance = d;
                    }
                    if (n == method + 16)
                        break;
                }
            }
            if (length >= method + 1) {
                U32 token = (U32)(((distance - 1) << 4) | (length - method - 1));
                packed.push_back((U8)token);
                packed.push_back((U8)(token >> 8));
                position += length;
            } else {
                packed[flagsAt] |= 1 << bit;
                packed.push_back(movieBytes[position++]);
            }
        }
    }
    archiveBytes.resize(18);
    WriteLE_U32(&archiveBytes[4], (U32)packed.size() + 18);
    WriteLE_U32(&archiveBytes[12], (U32)packed.size());
    WriteLE_U16(&archiveBytes[16], (U16)method);
    archiveBytes.insert(archiveBytes.end(), packed.begin(), packed.end());
}

static void compare_frame(smk expected, smk actual) {
    ASSERT_MEM_EQ(smk_get_video(expected), smk_get_video(actual), 320 * 200);
    ASSERT_MEM_EQ(smk_get_palette(expected), smk_get_palette(actual), 768);
    for (int track = 0; track < 7; ++track) {
        unsigned long size = smk_get_audio_size(expected, (unsigned char)track);
        ASSERT_EQ_UINT(size, smk_get_audio_size(actual, (unsigned char)track));
        if (size)
            ASSERT_MEM_EQ(smk_get_audio(expected, (unsigned char)track),
                          smk_get_audio(actual, (unsigned char)track), size);
    }
}

static void compare_movie(unsigned frames, S32 index = 0) {
    smk expected = smk_open_memory(&movieBytes[0], (unsigned long)movieBytes.size());
    smk actual = Video_OpenSource("VIDEO.HQR", index);
    ASSERT_TRUE(expected && actual);
    if (!expected || !actual)
        return;
    unsigned long ew, eh, ef, aw, ah, af;
    double et, at;
    smk_info_video(expected, &ew, &eh, NULL);
    smk_info_video(actual, &aw, &ah, NULL);
    smk_info_all(expected, NULL, &ef, &et);
    smk_info_all(actual, NULL, &af, &at);
    ASSERT_EQ_UINT(ew, aw);
    ASSERT_EQ_UINT(eh, ah);
    ASSERT_EQ_UINT(ef, af);
    ASSERT_TRUE(et == at);
    smk_enable_all(expected, 0xFF);
    smk_enable_all(actual, 0xFF);
    ASSERT_EQ_INT(smk_first(expected), smk_first(actual));
    ASSERT_EQ_UINT(1, smk_get_video(actual)[0]);
    for (unsigned i = 0; i < frames; ++i) {
        compare_frame(expected, actual);
        ASSERT_EQ_INT(smk_next(expected), smk_next(actual));
    }
    ASSERT_EQ_INT(smk_first(expected), smk_first(actual));
    compare_frame(expected, actual);
    ASSERT_EQ_INT(smk_seek_keyframe(expected, frames / 2), smk_seek_keyframe(actual, frames / 2));
    compare_frame(expected, actual);
    smk_close(expected);
    smk_close(actual);
    ASSERT_EQ_INT(0, openHandles);
    ASSERT_EQ_INT(0, errorLogs);
}

static void test_frames_audio_palette_and_rewind() {
    fixture(40);
    compare_movie(40);
}

static void test_compressed_sources_and_short_reads() {
    for (unsigned method = 1; method <= 2; ++method) {
        for (int matches = 0; matches <= 1; ++matches) {
            fixture(40, 4096);
            compress(method, matches != 0);
            maxRead = 17;
            compare_movie(40);
        }
    }
}

static void test_nonfirst_entry_and_stored_packed_size() {
    fixture(3);
    std::vector<U8> firstMovie = movieBytes;
    std::vector<U8> bank = archiveBytes;
    fixture(5);
    compress(2);
    WriteLE_U32(&bank[4], (U32)bank.size());
    bank.insert(bank.end(), archiveBytes.begin() + 8, archiveBytes.end());
    archiveBytes = bank;
    compare_movie(5, 1);
    movieBytes = firstMovie;
    WriteLE_U32(&archiveBytes[12], 0);
    compare_movie(3, 0);
}

static void test_open_does_not_read_whole_movie() {
    fixture(40, 65536);
    smk actual = Video_OpenSource("VIDEO.HQR", 0);
    ASSERT_TRUE(actual != NULL);
    ASSERT_TRUE(bytesRead <= 65536 + 32);
    ASSERT_TRUE(largestRead <= 65536);
    if (actual) {
        smk_enable_all(actual, 0xFF);
        ASSERT_TRUE(smk_first(actual) != SMK_ERROR);
        ASSERT_TRUE(bytesRead < movieBytes.size() / 4);
        smk_close(actual);
    }
    ASSERT_EQ_INT(0, openHandles);
}

static void test_compressed_open_and_first_frame_are_bounded() {
    for (unsigned method = 1; method <= 2; ++method) {
        fixture(40, 65536);
        compress(method);
        smk actual = Video_OpenSource("VIDEO.HQR", 0);
        ASSERT_TRUE(actual != NULL);
        ASSERT_TRUE(bytesRead < archiveBytes.size() / 4);
        ASSERT_TRUE(largestRead <= 4096);
        if (actual) {
            smk_enable_all(actual, 0xFF);
            ASSERT_TRUE(smk_first(actual) != SMK_ERROR);
            ASSERT_TRUE(bytesRead < archiveBytes.size() / 4);
            smk_close(actual);
        }
        ASSERT_EQ_INT(0, openHandles);
    }
}

static void test_failures_close_archive() {
    for (int scenario = 0; scenario < 11; ++scenario) {
        fixture(3);
        S32 index = 0;
        switch (scenario) {
        case 0:
            missingFile = true;
            break;
        case 1:
            index = -1;
            break;
        case 2:
            index = 2;
            break;
        case 3:
            archiveBytes.resize(3);
            break;
        case 4:
            WriteLE_U32(&archiveBytes[0], 7);
            break;
        case 5:
            WriteLE_U32(&archiveBytes[8], 0);
            break;
        case 6:
            WriteLE_U16(&archiveBytes[16], 3);
            break;
        case 7:
            archiveBytes.pop_back();
            break;
        case 8:
            archiveBytes[18] = 0;
            break;
        case 9:
            WriteLE_U32(&archiveBytes[18 + 104], 0x7FFFFFFD);
            break;
        case 10:
            failAt = 24;
            break;
        }
        smk actual = Video_OpenSource("VIDEO.HQR", index);
        ASSERT_TRUE(actual == NULL);
        if (actual)
            smk_close(actual);
        ASSERT_EQ_INT(0, openHandles);
        ASSERT_TRUE(errorLogs > 0);
    }
}

static void test_invalid_compressed_data() {
    for (unsigned method = 1; method <= 2; ++method) {
        fixture(3);
        compress(method, false);
        archiveBytes.pop_back();
        WriteLE_U32(&archiveBytes[12], (U32)archiveBytes.size() - 18);
        smk actual = Video_OpenSource("VIDEO.HQR", 0);
        ASSERT_TRUE(actual == NULL);
        if (actual)
            smk_close(actual);
        ASSERT_EQ_INT(0, openHandles);
        ASSERT_TRUE(errorLogs > 0);

        fixture(3);
        archiveBytes.resize(21, 0);
        WriteLE_U32(&archiveBytes[12], 3);
        WriteLE_U16(&archiveBytes[16], (U16)method);
        memset(&archiveBytes[18], 0, 3);
        actual = Video_OpenSource("VIDEO.HQR", 0);
        ASSERT_TRUE(actual == NULL);
        if (actual)
            smk_close(actual);
        ASSERT_EQ_INT(0, openHandles);
        ASSERT_TRUE(errorLogs > 0);
    }
}

static void test_playback_read_failure() {
    fixture(4, 65536);
    smk actual = Video_OpenSource("VIDEO.HQR", 0);
    ASSERT_TRUE(actual != NULL);
    if (!actual)
        return;
    smk_enable_all(actual, 0xFF);
    ASSERT_TRUE(smk_first(actual) != SMK_ERROR);
    failAt = filePosition;
    ASSERT_EQ_INT(SMK_ERROR, smk_next(actual));
    smk_close(actual);
    ASSERT_EQ_INT(0, openHandles);
    ASSERT_TRUE(errorLogs > 0);
}

static void test_filepointer_api_compatibility() {
    for (int mode = 0; mode <= 1; ++mode) {
        fixture(3);
        FILE *file = tmpfile();
        ASSERT_TRUE(file != NULL);
        if (!file)
            return;
        ASSERT_EQ_UINT(movieBytes.size(), fwrite(&movieBytes[0], 1, movieBytes.size(), file));
        rewind(file);
        smk actual = smk_open_filepointer(file, mode == 0 ? SMK_MODE_DISK : SMK_MODE_MEMORY);
        smk expected = smk_open_memory(&movieBytes[0], (unsigned long)movieBytes.size());
        ASSERT_TRUE(actual && expected);
        if (!actual || !expected)
            return;
        smk_enable_all(actual, 0xFF);
        smk_enable_all(expected, 0xFF);
        ASSERT_EQ_INT(smk_first(expected), smk_first(actual));
        compare_frame(expected, actual);
        smk_close(actual);
        smk_close(expected);
    }
}

static void test_randomized_movie_sizes_and_compression() {
    U32 state = 0xDEADBEEF;
    for (int round = 0; round < 30; ++round) {
        state = state * 1103515245u + 12345u;
        unsigned frames = 1 + ((state >> 16) & 31);
        state = state * 1103515245u + 12345u;
        unsigned chunkBytes = 36 + (((state >> 16) & 1023) * 4);
        fixture(frames, chunkBytes);
        if (round % 3)
            compress(round % 3);
        if (round % 4 == 0)
            maxRead = 1;
        compare_movie(frames);
    }
}

struct CallbackSource {
    size_t position;
    int closes;
    bool failSeek;
};

static size_t callback_read(void *userdata, void *out, size_t bytes) {
    CallbackSource *source = (CallbackSource *)userdata;
    size_t n = std::min(bytes, movieBytes.size() - source->position);
    if (n)
        memcpy(out, &movieBytes[source->position], n);
    source->position += n;
    return n;
}
static int callback_seek(void *userdata, long offset, int origin) {
    CallbackSource *source = (CallbackSource *)userdata;
    if (source->failSeek)
        return -1;
    long base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? (long)source->position
                                                            : (long)movieBytes.size();
    long position = base + offset;
    if (position < 0 || (size_t)position > movieBytes.size())
        return -1;
    source->position = (size_t)position;
    return 0;
}
static long callback_tell(void *userdata) {
    return (long)((CallbackSource *)userdata)->position;
}
static void callback_close(void *userdata) {
    ++((CallbackSource *)userdata)->closes;
}

static void test_source_seek_failure_status() {
    fixture(3);
    CallbackSource source = {0, 0, false};
    const smk_io io = {&source, callback_read, callback_seek, callback_tell, callback_close};
    smk actual = smk_open_stream(&io);
    ASSERT_TRUE(actual != NULL);
    if (!actual)
        return;
    ASSERT_EQ_INT(0, source.closes);
    smk_enable_all(actual, 0xFF);
    ASSERT_TRUE(smk_first(actual) != SMK_ERROR);
    source.failSeek = true;
    ASSERT_EQ_INT(SMK_ERROR, smk_next(actual));
    ASSERT_EQ_INT(SMK_ERROR, smk_first(actual));
    ASSERT_EQ_INT(SMK_ERROR, smk_seek_keyframe(actual, 1));
    smk_close(actual);
    ASSERT_EQ_INT(1, source.closes);
}

int main() {
    RUN_TEST(test_source_seek_failure_status);
    if (test_failures) {
        TEST_SUMMARY();
        return 1;
    }
    RUN_TEST(test_frames_audio_palette_and_rewind);
    RUN_TEST(test_open_does_not_read_whole_movie);
    RUN_TEST(test_compressed_sources_and_short_reads);
    RUN_TEST(test_nonfirst_entry_and_stored_packed_size);
    RUN_TEST(test_compressed_open_and_first_frame_are_bounded);
    RUN_TEST(test_failures_close_archive);
    RUN_TEST(test_invalid_compressed_data);
    RUN_TEST(test_playback_read_failure);
    RUN_TEST(test_filepointer_api_compatibility);
    RUN_TEST(test_randomized_movie_sizes_and_compression);
    TEST_SUMMARY();
    return test_failures != 0;
}
