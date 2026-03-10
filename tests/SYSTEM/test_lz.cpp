/* Test: ExpandLZ — LZ decompression */
#include "test_harness.h"

#include <SYSTEM/LZ.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_ExpandLZ(void *Dst, void *Src, U32 DecompSize, U32 MinBloc);
#endif

static void test_all_literals(void)
{
    /* Construct a minimal LZ stream: all literal bytes.
       Flag byte 0xFF = all 8 bits set (literal), then 8 data bytes. */
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
    /* Create a stream that copies literals then back-references the first.
       Flag 0x07 = bits 0,1,2 set (3 literals), remaining bits=0 (backrefs).
       At 3 bytes decompressed we stop, so we only use 3 literal bits. */
    U8 src[] = {0x07, 'A', 'B', 'C'};
    U8 dst[3] = {0};
    ExpandLZ(dst, src, 3, 2);
    ASSERT_EQ_INT('A', dst[0]);
    ASSERT_EQ_INT('B', dst[1]);
    ASSERT_EQ_INT('C', dst[2]);
}

static void test_decomp_size_respected(void)
{
    /* Only decompress 4 bytes even though stream has more data */
    U8 src[] = {0xFF, 'A','B','C','D','E','F','G','H'};
    U8 dst[8];
    memset(dst, 0xFF, sizeof(dst));
    ExpandLZ(dst, src, 4, 2);
    ASSERT_EQ_INT('A', dst[0]);
    ASSERT_EQ_INT('D', dst[3]);
    /* Byte 5 should not have been written */
    ASSERT_EQ_INT(0xFF, dst[4]);
}

#ifdef LBA2_ASM_TESTS
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
#endif

int main(void)
{
    RUN_TEST(test_all_literals);
    RUN_TEST(test_single_literal);
    RUN_TEST(test_back_reference);
    RUN_TEST(test_decomp_size_respected);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
