/* Test: CopyBlockIncrust — copy block with incrustation (transparency) */
#include <SVGA/CPYBLOCI.H>

#include "cpyblock_sweep.h"

extern "C" void asm_CopyBlockIncrust(void);

static void call_asm_CopyBlockIncrust(S32 x0, S32 y0, S32 x1, S32 y1, void *src,
                                      S32 xd, S32 yd, void *dst) {
    __asm__ __volatile__(
        "push %[dst]\n\t"
        "push %[yd]\n\t"
        "push %[xd]\n\t"
        "mov %[src], %%esi\n\t"
        "mov %[y1], %%edx\n\t"
        "mov %[x1], %%ecx\n\t"
        "mov %[y0], %%ebx\n\t"
        "mov %[x0], %%eax\n\t"
        "call asm_CopyBlockIncrust\n\t"
        "add $12, %%esp"
        :
        : [x0] "g"(x0), [y0] "g"(y0), [x1] "g"(x1), [y1] "g"(y1), [src] "g"(src),
          [xd] "g"(xd), [yd] "g"(yd), [dst] "g"(dst)
        : "eax", "ebx", "ecx", "edx", "esi", "edi", "memory", "cc");
}

/* Transparent zeros scattered through the source; every other source byte
 * below 0x80 and every destination byte at or above it (cpyblock_sweep.h). */
static void fill_source_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        U8 value = (U8)(((i * 37u) + (salt * 53u) + (i >> 3)) & 0x7Fu);
        if (((i + salt) % 7u) == 0u || ((i ^ salt) & 0x1Fu) == 0u) {
            value = 0;
        }
        src_init[i] = value;
    }
}

static void fill_dest_pattern(U32 salt) {
    for (U32 i = 0; i < kBufferSize; ++i) {
        dst_init[i] = (U8)(0x80u | (((i * 19u) + (salt * 11u) + 0x5Au) & 0x7Fu));
    }
}

static void test_equivalence(void) {
    assert_blit_case("CopyBlockIncrust fixed transparency",
                     10, 10, 11, 10, 50, 50,
                     0, 0, 639, 479, 1u);
    assert_blit_case("CopyBlockIncrust fixed clipped source",
                     -8, -3, 12, 6, 40, 20,
                     0, 0, 639, 479, 2u);
    assert_blit_case("CopyBlockIncrust fixed clipped destination",
                     20, 20, 80, 45, 610, 460,
                     0, 0, 639, 479, 3u);
    assert_blit_case("CopyBlockIncrust fixed custom clip",
                     50, 30, 120, 70, 180, 100,
                     64, 32, 255, 191, 4u);
    assert_blit_case("CopyBlockIncrust fixed outside clip",
                     100, 100, 140, 120, 20, 20,
                     200, 200, 300, 260, 5u);
}

int main(void) {
    blit_sweep_use(CopyBlockIncrust, call_asm_CopyBlockIncrust, "CopyBlockIncrust");
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_edge_sweep_brick);
    RUN_TEST(test_edge_sweep_large);
    RUN_TEST(test_edge_sweep_fullscreen);
    TEST_SUMMARY();
    return test_failures != 0;
}
