#include "test_harness.h"

#include <FUNC.H>

#include <string.h>

extern "C" U8 asm_SearchBoundColRGB(U8 Rouge, U8 Vert, U8 Bleu, U8 *Palette,
                                      U8 coulmin, U8 coulmax);

static U32 rng_state;

static void rng_seed(U32 seed_value)
{
    rng_state = seed_value;
}

static U32 rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void run_searchboundcolrgb_case(const char *label,
                                       U8 red, U8 green, U8 blue,
                                       const U8 *palette_src,
                                       U8 coulmin, U8 coulmax,
                                       U32 palette_size)
{
    U8 cpp_palette[256 * 3];
    U8 asm_palette[256 * 3];

    ASSERT_TRUE(palette_size <= sizeof(cpp_palette));

    memcpy(cpp_palette, palette_src, palette_size);
    memcpy(asm_palette, palette_src, palette_size);

    U8 cpp_result = SearchBoundColRGB(red, green, blue, cpp_palette, coulmin, coulmax);
    U8 asm_result = asm_SearchBoundColRGB(red, green, blue, asm_palette, coulmin, coulmax);

    ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_palette, cpp_palette, palette_size, label);
}

static void test_searchboundcolrgb_fixed_cases(void)
{
    static const U8 palette[] = {
        0, 0, 0,
        10, 20, 30,
        40, 50, 60,
        80, 90, 100,
        120, 130, 140,
        200, 210, 220,
        240, 245, 250,
        255, 255, 255
    };

    run_searchboundcolrgb_case("SearchBoundColRGB exact middle",
                               40, 50, 60, palette, 0, 7, sizeof(palette));
    run_searchboundcolrgb_case("SearchBoundColRGB subrange",
                               118, 129, 141, palette, 2, 5, sizeof(palette));
    run_searchboundcolrgb_case("SearchBoundColRGB upper bound",
                               254, 254, 254, palette, 0, 7, sizeof(palette));
}

static void test_searchboundcolrgb_edge_cases(void)
{
    static const U8 palette[] = {
        5, 10, 15,
        25, 35, 45,
        100, 110, 120,
        180, 190, 200,
        220, 221, 222,
        250, 251, 252
    };

    run_searchboundcolrgb_case("SearchBoundColRGB single entry",
                               17, 33, 49, palette, 3, 3, sizeof(palette));
    run_searchboundcolrgb_case("SearchBoundColRGB min greater than max",
                               90, 100, 110, palette, 4, 2, sizeof(palette));
}

static void test_searchboundcolrgb_random_stress(void)
{
    U8 palette[64 * 3];

    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 300; ++round)
    {
        for (U32 i = 0; i < sizeof(palette); ++i)
        {
            palette[i] = (U8)rng_next();
        }

        U8 red = (U8)rng_next();
        U8 green = (U8)rng_next();
        U8 blue = (U8)rng_next();
        U8 coulmin = (U8)(rng_next() % 64);
        U8 coulmax = (U8)(rng_next() % 64);

        run_searchboundcolrgb_case("SearchBoundColRGB random",
                                   red, green, blue, palette,
                                   coulmin, coulmax, sizeof(palette));
    }
}

int main(void)
{
    RUN_TEST(test_searchboundcolrgb_fixed_cases);
    RUN_TEST(test_searchboundcolrgb_edge_cases);
    RUN_TEST(test_searchboundcolrgb_random_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}