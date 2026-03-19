/* Test: FastCopy — fast memory copy */
#include "test_harness.h"

#include <SYSTEM/FASTCPY.H>
#include <stdio.h>
#include <string.h>

extern "C" void asm_FastCopyF(void *dst, void *src, U32 len);

static const U32 kBufferSize = 1024;

static U8 src_init[kBufferSize];
static U8 cpp_src[kBufferSize];
static U8 asm_src[kBufferSize];
static U8 dst_init[kBufferSize];
static U8 cpp_dst[kBufferSize];
static U8 asm_dst[kBufferSize];
static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void fill_source_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        U8 value = (U8)(((i * 37u) + (salt * 13u) + (i >> 1)) & 0xFFu);
        if (((i + salt) & 0x0Fu) == 0u) {
            value = 0xFFu;
        }
        src_init[i] = value;
    }
}

static void fill_dest_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        dst_init[i] = (U8)(((i * 11u) + (salt * 29u) + 0x55u) & 0xFFu);
    }
}

static void assert_fastcopy_case(const char *label, U32 dst_offset, U32 src_offset,
                                 U32 len, U32 salt) {
    fill_source_pattern(salt);
    fill_dest_pattern(salt + 1u);
    memcpy(cpp_src, src_init, sizeof(cpp_src));
    memcpy(asm_src, src_init, sizeof(asm_src));
    memcpy(cpp_dst, dst_init, sizeof(cpp_dst));
    memcpy(asm_dst, dst_init, sizeof(asm_dst));

    FastCopy(cpp_dst + dst_offset, cpp_src + src_offset, len);
    asm_FastCopyF(asm_dst + dst_offset, asm_src + src_offset, len);

    ASSERT_ASM_CPP_MEM_EQ(src_init, cpp_src, sizeof(cpp_src), label);
    ASSERT_ASM_CPP_MEM_EQ(src_init, asm_src, sizeof(asm_src), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), label);
}

static void test_zero_length(void) {
    assert_fastcopy_case("FastCopy zero length", 0, 0, 0, 1u);
}

static void test_fixed_equivalence(void) {
    assert_fastcopy_case("FastCopy len1", 0, 0, 1, 2u);
    assert_fastcopy_case("FastCopy len4", 3, 5, 4, 3u);
    assert_fastcopy_case("FastCopy len7", 1, 2, 7, 4u);
    assert_fastcopy_case("FastCopy len8 aligned", 8, 16, 8, 5u);
    assert_fastcopy_case("FastCopy len15", 5, 9, 15, 6u);
    assert_fastcopy_case("FastCopy len16 aligned", 16, 32, 16, 7u);
    assert_fastcopy_case("FastCopy len17", 7, 11, 17, 8u);
    assert_fastcopy_case("FastCopy len64", 32, 48, 64, 9u);
    assert_fastcopy_case("FastCopy len255", 13, 21, 255, 10u);
    assert_fastcopy_case("FastCopy len512", 64, 96, 512, 11u);
    assert_fastcopy_case("FastCopy len1024 window", 0, 0, 1024, 12u);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 200; ++i) {
        U32 len = rng_next() % (kBufferSize + 1u);
        U32 max_src_offset = kBufferSize - len;
        U32 max_dst_offset = kBufferSize - len;
        U32 src_offset = max_src_offset == 0u ? 0u : (rng_next() % (max_src_offset + 1u));
        U32 dst_offset = max_dst_offset == 0u ? 0u : (rng_next() % (max_dst_offset + 1u));
        char label[64];

        snprintf(label, sizeof(label), "FastCopy rand %d", i);
        assert_fastcopy_case(label, dst_offset, src_offset, len, (U32)i + 100u);
    }
}

int main(void) {
    RUN_TEST(test_zero_length);
    RUN_TEST(test_fixed_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
