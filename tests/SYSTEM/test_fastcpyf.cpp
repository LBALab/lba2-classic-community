/* Test: FastCopy — fast memory copy */
#include "test_harness.h"

#include <SYSTEM/FASTCPY.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_FastCopy(void *dst, void *src, U32 len);
#endif

static void test_zero_length(void)
{
    U8 dst[16];
    U8 src[16];
    memset(dst, 0xFF, 16);
    memset(src, 0xAA, 16);
    FastCopy(dst, src, 0);
    /* Nothing should be copied */
    ASSERT_EQ_UINT(0xFF, dst[0]);
}

static void test_small_copy(void)
{
    U8 dst[4] = {0};
    U8 src[4] = {1, 2, 3, 4};
    FastCopy(dst, src, 4);
    ASSERT_MEM_EQ(src, dst, 4);
}

static void test_large_copy(void)
{
    U8 dst[1024];
    U8 src[1024];
    for (int i = 0; i < 1024; i++) src[i] = (U8)(i & 0xFF);
    memset(dst, 0, 1024);
    FastCopy(dst, src, 1024);
    ASSERT_MEM_EQ(src, dst, 1024);
}

static void test_odd_size(void)
{
    U8 dst[7] = {0};
    U8 src[7] = {10, 20, 30, 40, 50, 60, 70};
    FastCopy(dst, src, 7);
    ASSERT_MEM_EQ(src, dst, 7);
}

static void test_single_byte(void)
{
    U8 dst = 0;
    U8 src = 0x42;
    FastCopy(&dst, &src, 1);
    ASSERT_EQ_UINT(0x42, dst);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    U8 src[256];
    for (int i = 0; i < 256; i++) src[i] = (U8)i;
    U32 sizes[] = {0, 1, 4, 7, 16, 17, 64, 255, 256};
    int n = sizeof(sizes) / sizeof(sizes[0]);
    for (int i = 0; i < n; i++) {
        U8 dst_cpp[256], dst_asm[256];
        memset(dst_cpp, 0xFF, 256);
        memset(dst_asm, 0xFF, 256);
        FastCopy(dst_cpp, src, sizes[i]);
        asm_FastCopy(dst_asm, src, sizes[i]);
        ASSERT_ASM_CPP_MEM_EQ(dst_asm, dst_cpp, 256, "FastCopy");
    }
}
#endif

int main(void)
{
    RUN_TEST(test_zero_length);
    RUN_TEST(test_small_copy);
    RUN_TEST(test_large_copy);
    RUN_TEST(test_odd_size);
    RUN_TEST(test_single_byte);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
