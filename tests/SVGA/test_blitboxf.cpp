/* Test: BlitBoxF — fast blit rectangular region */
#include "test_harness.h"

#include <SVGA/BLITBOX.H>
#include <SVGA/VIDEO.H>

#include <stdlib.h>
#include <string.h>

extern void BlitBoxF(void *dst, void *src);
extern "C" void asm_BlitBoxF(void *dst, void *src);

static U32 rng_state;

enum { PITCH = 640,
       HEIGHT = 480,
       DST_SIZE = PITCH * HEIGHT,
       SRC_SIZE = 320 * 200 };

static U8 src_seed[SRC_SIZE];
static U8 dst_seed[DST_SIZE];

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void fill_random_bytes(U8 *buffer, size_t size) {
    for (size_t index = 0; index < size; ++index) {
        buffer[index] = (U8)(rng_next() & 0xFFu);
    }
}

static void init_offsets(U32 pitch) {
    for (U32 y = 0; y < ADELINE_MAX_Y_RES; ++y) {
        TabOffPhysLine[y] = y * pitch;
    }
}

static void assert_blitboxf_case(const char *label, const U8 *src_init, const U8 *dst_init) {
    U8 *cpp_dst = (U8 *)malloc(DST_SIZE);
    U8 *asm_dst = (U8 *)malloc(DST_SIZE);
    U8 *cpp_src = (U8 *)malloc(SRC_SIZE);
    U8 *asm_src = (U8 *)malloc(SRC_SIZE);

    ASSERT_TRUE(cpp_dst != NULL);
    ASSERT_TRUE(asm_dst != NULL);
    ASSERT_TRUE(cpp_src != NULL);
    ASSERT_TRUE(asm_src != NULL);

    memcpy(cpp_dst, dst_init, DST_SIZE);
    memcpy(asm_dst, dst_init, DST_SIZE);
    memcpy(cpp_src, src_init, SRC_SIZE);
    memcpy(asm_src, src_init, SRC_SIZE);

    init_offsets(PITCH);
    BlitBoxF(cpp_dst, cpp_src);

    init_offsets(PITCH);
    asm_BlitBoxF(asm_dst, asm_src);

    if (memcmp(asm_dst, cpp_dst, DST_SIZE) != 0) {
        size_t diff_index;
        for (diff_index = 0; diff_index < DST_SIZE; ++diff_index) {
            if (asm_dst[diff_index] != cpp_dst[diff_index]) {
                break;
            }
        }
        FAIL_MSG("[%s] first dst diff at %d: ASM=%u CPP=%u",
                 label, (int)diff_index, (unsigned)asm_dst[diff_index], (unsigned)cpp_dst[diff_index]);
    }
    ASSERT_ASM_CPP_MEM_EQ(src_init, cpp_src, SRC_SIZE, label);
    ASSERT_ASM_CPP_MEM_EQ(src_init, asm_src, SRC_SIZE, label);

    free(cpp_dst);
    free(asm_dst);
    free(cpp_src);
    free(asm_src);
}

static void test_equivalence(void) {
    memset(src_seed, 0x3C, sizeof(src_seed));
    memset(dst_seed, 0xA5, sizeof(dst_seed));
    assert_blitboxf_case("BlitBoxF fixed solid fill", src_seed, dst_seed);

    for (size_t index = 0; index < sizeof(src_seed); ++index) {
        src_seed[index] = (U8)(index & 0xFFu);
    }
    memset(dst_seed, 0x5A, sizeof(dst_seed));
    assert_blitboxf_case("BlitBoxF fixed gradient", src_seed, dst_seed);
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 20; ++i) {
        char label[64];

        fill_random_bytes(src_seed, sizeof(src_seed));
        fill_random_bytes(dst_seed, sizeof(dst_seed));
        snprintf(label, sizeof(label), "BlitBoxF rand %d", i);
        assert_blitboxf_case(label, src_seed, dst_seed);
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
