/* Test: CopyBlock — fast memory block copy between screen regions */
#include <SVGA/CPYBLOCK.H>

#include "cpyblock_sweep.h"

extern "C" void asm_CopyBlock(S32 x0, S32 y0, S32 x1, S32 y1, void *src,
                              S32 xd, S32 yd, void *dst);

/* Source below 0x80, destination at or above it: see cpyblock_sweep.h. */
static void fill_source_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        src_init[i] = (U8)(((i * 29u) + (salt * 17u) + (i >> 2)) & 0x7Fu);
    }
}

static void fill_dest_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        dst_init[i] = (U8)(0x80u | (((i * 13u) + (salt * 31u) + 0x66u) & 0x7Fu));
    }
}

static void test_equivalence(void) {
    assert_blit_case("CopyBlock fixed simple",
                     10, 10, 20, 20, 50, 50,
                     0, 0, 639, 479, 1u);
    assert_blit_case("CopyBlock fixed clipped source",
                     -8, -3, 12, 6, 40, 20,
                     0, 0, 639, 479, 2u);
    assert_blit_case("CopyBlock fixed clipped destination",
                     20, 20, 80, 45, 610, 460,
                     0, 0, 639, 479, 3u);
    assert_blit_case("CopyBlock fixed custom clip",
                     50, 30, 120, 70, 180, 100,
                     64, 32, 255, 191, 4u);
    assert_blit_case("CopyBlock fixed outside clip",
                     100, 100, 140, 120, 20, 20,
                     200, 200, 300, 260, 5u);
}

int main(void) {
    blit_sweep_use(CopyBlock, asm_CopyBlock, "CopyBlock");
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_edge_sweep_brick);
    RUN_TEST(test_edge_sweep_large);
    RUN_TEST(test_edge_sweep_fullscreen);
    TEST_SUMMARY();
    return test_failures != 0;
}
