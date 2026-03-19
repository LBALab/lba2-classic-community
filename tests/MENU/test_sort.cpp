/* Test: MySortCompFunc — comparison function for qsort (strcmp-based) */
#include "test_harness.h"

#include <SYSTEM/ADELINE_TYPES.H>
#include <MENU/SELECTOR.H>
#include <stdio.h>
#include <string.h>

extern "C" int asm_MySortCompFunc(const void *ptra, const void *ptrb);

static U32 rng_state;

static void rng_seed(U32 seed) {
    rng_state = seed;
}

static U32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void assert_sort_case(const char *label, const char *a, const char *b) {
    int asm_result = asm_MySortCompFunc(&a, &b);
    int cpp_result = MySortCompFunc(&a, &b);
    ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, label);
}

static void test_equal_strings(void) {
    assert_sort_case("MySortCompFunc equal", "hello", "hello");
}

static void test_a_less_than_b(void) {
    assert_sort_case("MySortCompFunc less", "alpha", "beta");
}

static void test_a_greater_than_b(void) {
    assert_sort_case("MySortCompFunc greater", "beta", "alpha");
}

static void test_fixed_equivalence(void) {
    assert_sort_case("MySortCompFunc prefix", "alpha", "alphabet");
    assert_sort_case("MySortCompFunc empty", "", "zeta");
    assert_sort_case("MySortCompFunc punctuation", "file-10", "file_2");
    assert_sort_case("MySortCompFunc case", "Bravo", "bravo");
}

static void build_random_string(char *dst, U32 max_len) {
    U32 len = rng_next() % max_len;
    for (U32 i = 0; i < len; ++i) {
        dst[i] = (char)('a' + (rng_next() % 26u));
    }
    dst[len] = '\0';
}

static void test_random_equivalence(void) {
    rng_seed(0xDEADBEEFu);
    for (int i = 0; i < 100; ++i) {
        char a[32];
        char b[32];
        char label[64];

        build_random_string(a, sizeof(a) - 1u);
        build_random_string(b, sizeof(b) - 1u);
        snprintf(label, sizeof(label), "MySortCompFunc rand %d", i);
        assert_sort_case(label, a, b);
    }
}

int main(void) {
    RUN_TEST(test_equal_strings);
    RUN_TEST(test_a_less_than_b);
    RUN_TEST(test_a_greater_than_b);
    RUN_TEST(test_fixed_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
