/* Test: ExpandLZ — LZ decompression */
#include "test_harness.h"

#include <SYSTEM/LZ.H>
#include <string.h>
#include <stdlib.h>

extern "C" void asm_ExpandLZ(void *Dst, void *Src, U32 DecompSize, U32 MinBloc);

/*
 * Minimal LZ77 compressor matching the ExpandLZ format.
 * Only used for test data generation — not optimized.
 *
 * Format: flag bytes where each bit indicates literal (1) or
 * back-reference (0).  Back-refs are 2 bytes encoding a 12-bit
 * offset and 4-bit length: byte0 = (offset_lo<<4)|length,
 * byte1 = offset_hi.  Actual length = encoded_length + minBloc.
 */
static U32 compress_lz(U8 *dst, const U8 *src, U32 srcLen, U32 minBloc)
{
    U32 si = 0, di = 0;

    while (si < srcLen)
    {
        U32 flagPos = di++;
        U8 flag = 0;

        for (int bit = 0; bit < 8 && si < srcLen; bit++)
        {
            /* Try to find a back-reference */
            U32 bestLen = 0, bestOff = 0;
            U32 maxOff = si < 4096 ? si : 4096;

            for (U32 off = 1; off <= maxOff; off++)
            {
                U32 len = 0;
                while (len < 15 + minBloc && si + len < srcLen &&
                       src[si + len] == src[si - off + (len % off)])
                    len++;
                if (len >= minBloc && len > bestLen)
                {
                    bestLen = len;
                    bestOff = off;
                }
            }

            if (bestLen >= minBloc)
            {
                /* Back-reference: offset is stored as (offset-1) */
                U32 encOff = bestOff - 1;
                U32 encLen = bestLen - minBloc;
                dst[di++] = (U8)((encOff << 4) | (encLen & 0x0F));
                dst[di++] = (U8)(encOff >> 4);
                si += bestLen;
                /* bit stays 0 (back-ref) */
            }
            else
            {
                /* Literal */
                flag |= (1 << bit);
                dst[di++] = src[si++];
            }
        }
        dst[flagPos] = flag;
    }
    return di;
}

static void test_all_literals(void)
{
    U8 src[] = {0xFF, 'A','B','C','D','E','F','G','H'};
    U8 dst[8];
    memset(dst, 0, sizeof(dst));
    ExpandLZ(dst, src, 8, 2);
    ASSERT_EQ_INT('A', dst[0]);
    ASSERT_EQ_INT('H', dst[7]);
}

static void test_single_literal(void)
{
    U8 src[] = {0x01, 'X'};
    U8 dst[1] = {0};
    ExpandLZ(dst, src, 1, 2);
    ASSERT_EQ_INT('X', dst[0]);
}

static void test_back_reference(void)
{
    U8 src[] = {0x07, 'A', 'B', 'C'};
    U8 dst[3] = {0};
    ExpandLZ(dst, src, 3, 2);
    ASSERT_EQ_INT('A', dst[0]);
    ASSERT_EQ_INT('B', dst[1]);
    ASSERT_EQ_INT('C', dst[2]);
}

static void test_decomp_size_respected(void)
{
    U8 src[] = {0xFF, 'A','B','C','D','E','F','G','H'};
    U8 dst[8];
    memset(dst, 0xFF, sizeof(dst));
    ExpandLZ(dst, src, 4, 2);
    ASSERT_EQ_INT('A', dst[0]);
    ASSERT_EQ_INT('D', dst[3]);
    ASSERT_EQ_INT(0xFF, dst[4]);
}

static void test_asm_equiv(void)
{
    U8 src[] = {0xFF, 'H','e','l','l','o','!','!','!'};
    U8 dst_cpp[8], dst_asm[8];
    memset(dst_cpp, 0, sizeof(dst_cpp));
    memset(dst_asm, 0, sizeof(dst_asm));
    ExpandLZ(dst_cpp, src, 8, 2);
    asm_ExpandLZ(dst_asm, src, 8, 2);
    ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp, 8, "ExpandLZ");
}

static void test_random_roundtrip(void)
{
    srand(42);
    for (int t = 0; t < 200; t++)
    {
        /* Generate random data with some repetition to exercise back-refs */
        U32 len = 16 + (rand() % 200);
        U8 orig[256];
        U8 alphabet_size = 2 + (rand() % 10);
        for (U32 i = 0; i < len; i++)
            orig[i] = (U8)(rand() % alphabet_size);

        /* Compress */
        U8 compressed[2048];
        U32 compLen = compress_lz(compressed, orig, len, 2);
        (void)compLen;

        /* Decompress with CPP and ASM */
        U8 dec_cpp[256], dec_asm[256];
        memset(dec_cpp, 0xCC, sizeof(dec_cpp));
        memset(dec_asm, 0xCC, sizeof(dec_asm));

        ExpandLZ(dec_cpp, compressed, len, 2);
        asm_ExpandLZ(dec_asm, compressed, len, 2);

        /* CPP roundtrip must match original */
        ASSERT_MEM_EQ(orig, dec_cpp, len);
        /* ASM must match CPP */
        ASSERT_ASM_CPP_MEM_EQ(dec_asm, dec_cpp, len, "ExpandLZ rand");
    }
}

static void test_random_roundtrip_mit(void)
{
    srand(99);
    for (int t = 0; t < 200; t++)
    {
        U32 len = 16 + (rand() % 200);
        U8 orig[256];
        U8 alphabet_size = 2 + (rand() % 10);
        for (U32 i = 0; i < len; i++)
            orig[i] = (U8)(rand() % alphabet_size);

        U8 compressed[2048];
        compress_lz(compressed, orig, len, 3);

        U8 dec_cpp[256], dec_asm[256];
        memset(dec_cpp, 0xCC, sizeof(dec_cpp));
        memset(dec_asm, 0xCC, sizeof(dec_asm));

        ExpandLZ(dec_cpp, compressed, len, 3);
        asm_ExpandLZ(dec_asm, compressed, len, 3);

        ASSERT_MEM_EQ(orig, dec_cpp, len);
        ASSERT_ASM_CPP_MEM_EQ(dec_asm, dec_cpp, len, "ExpandLZMIT rand");
    }
}

int main(void)
{
    RUN_TEST(test_all_literals);
    RUN_TEST(test_single_literal);
    RUN_TEST(test_back_reference);
    RUN_TEST(test_decomp_size_respected);
    RUN_TEST(test_asm_equiv);
    RUN_TEST(test_random_roundtrip);
    RUN_TEST(test_random_roundtrip_mit);
    TEST_SUMMARY();
    return test_failures != 0;
}
