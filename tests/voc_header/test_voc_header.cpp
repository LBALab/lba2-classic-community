#include "test_harness.h"

#include "VOC_HEADER.H"

#include <string.h>

/* Build a minimal VOC: a 26-byte header (first byte `firstByte`, then the 19-byte signature at
   offset 1), followed by one type-0x01 sound-data block carrying `payloadLen` payload bytes.
   `freqDiv`/`codec` are the VOC block's rate divisor and codec. Total size returned via *outSize. */
static void build_voc(U8 *buf, int *outSize, U8 firstByte, U8 freqDiv, U8 codec, int payloadLen) {
    memset(buf, 0, 32 + payloadLen);
    buf[0] = firstByte;
    memcpy(buf + 1, "reative Voice File\x1A", 19);
    /* bytes 20..25: the rest of the standard VOC header; not parsed. */
    int pos = 26;
    int blockSize = 2 + payloadLen; /* freqDiv + codec + payload */
    buf[pos] = 0x01;                /* block type: sound data */
    buf[pos + 1] = (U8)(blockSize & 0xFF);
    buf[pos + 2] = (U8)((blockSize >> 8) & 0xFF);
    buf[pos + 3] = (U8)((blockSize >> 16) & 0xFF);
    buf[pos + 4] = freqDiv;
    buf[pos + 5] = codec;
    *outSize = pos + 4 + blockSize;
}

/* The first byte is not a discriminator: 'C' (standard VOC) and the 0x00 / 0x01 / 'W' variants
   seen in LBA voice banks must all parse via the offset-1 signature. */
static void test_first_byte_variants_accepted(void) {
    const U8 variants[] = {(U8)'C', 0x00, 0x01, (U8)'W'};
    for (size_t i = 0; i < sizeof(variants); i++) {
        U8 buf[64];
        int size = 0;
        build_voc(buf, &size, variants[i], 0xA6 /* 11111 Hz */, 0 /* 8-bit PCM */, 8);
        const U8 *data = 0;
        S32 payload = 0, hz = 0;
        int is8 = 0;
        ASSERT_EQ_INT(1, ParseVocHeader(buf, size, &data, &payload, &hz, &is8, 44100));
        ASSERT_EQ_INT(11111, hz);
        ASSERT_EQ_INT(1, is8);
        ASSERT_EQ_INT(8, payload);
        ASSERT_TRUE(data == buf + 26 + 6);
    }
}

/* A non-zero codec is not 8-bit PCM. */
static void test_codec_flag(void) {
    U8 buf[64];
    int size = 0;
    build_voc(buf, &size, (U8)'C', 0xA6, 4 /* non-zero codec */, 4);
    const U8 *data = 0;
    S32 payload = 0, hz = 0;
    int is8 = 1;
    ASSERT_EQ_INT(1, ParseVocHeader(buf, size, &data, &payload, &hz, &is8, 44100));
    ASSERT_EQ_INT(0, is8);
}

/* Non-VOC (RIFF/WAV) and too-short buffers are rejected. */
static void test_non_voc_rejected(void) {
    const U8 *data = 0;
    S32 payload = 0, hz = 0;
    int is8 = 0;

    U8 riff[64];
    memset(riff, 0, sizeof(riff));
    memcpy(riff, "RIFF", 4);
    memcpy(riff + 8, "WAVE", 4);
    ASSERT_EQ_INT(0, ParseVocHeader(riff, (S32)sizeof(riff), &data, &payload, &hz, &is8, 44100));

    U8 tiny[10] = {(U8)'C'};
    ASSERT_EQ_INT(0, ParseVocHeader(tiny, (S32)sizeof(tiny), &data, &payload, &hz, &is8, 44100));
}

int main(void) {
    RUN_TEST(test_first_byte_variants_accepted);
    RUN_TEST(test_codec_flag);
    RUN_TEST(test_non_voc_rejected);
    TEST_SUMMARY();
    return test_failures != 0;
}
