/* Host test for LIB386/SYSTEM/QSORT.CPP, the qsort of the 1997 game.
 *
 * What RetailQsort is for is the order of equal elements, which every C library
 * chooses differently. The expected orders below were taken by running the
 * original code, the Watcom qsort at 0x63ed6 in the DOS LBA2.DOS, under a CPU
 * emulator on the same inputs, with the scenery comparator at 0x64f30 (second
 * depth minus first). The sizes reach each branch of the original: the
 * insertion pass (12), the middle pivot (20), the median of three (35) and the
 * median of three medians (60).
 */

#include <cstdio>
#include <cstdlib>

#include "test_harness.h"

#include <SYSTEM/QSORT.H>

/* The layout of the scenery's sort records in the 1997 build: depth first. */
struct Rec {
    int depth;
    int pad;
    int index;
    int pad2;
};

static int deeper_first(const void *a, const void *b) {
    return ((const Rec *)b)->depth - ((const Rec *)a)->depth;
}

/* Four depths, scrambled, so most elements tie with others. */
static int depth_of(unsigned i) {
    return (int)(((i * 2654435761u) & 0xFFFFFFFFu) >> 30) * 100;
}

static void sort_n(Rec *r, int n) {
    for (int i = 0; i < n; i++) {
        r[i].depth = depth_of((unsigned)i);
        r[i].pad = r[i].pad2 = 0;
        r[i].index = i;
    }
    RetailQsort(r, (size_t)n, sizeof *r, deeper_first);
}

static void check_order(int n, const int *expected) {
    Rec r[64];
    sort_n(r, n);
    for (int i = 0; i < n; i++)
        ASSERT_EQ_INT(expected[i], r[i].index);
}

static void test_insertion_pass(void) {
    static const int expected[] = {3, 8, 11, 1, 6, 9, 4, 7, 2, 5, 0, 10};
    check_order(12, expected);
}

static void test_middle_pivot(void) {
    static const int expected[] = {16, 8, 3, 11, 14, 17, 1, 19, 6, 9,
                                   15, 7, 12, 4, 10, 2, 5, 0, 13, 18};
    check_order(20, expected);
}

static void test_median_of_three(void) {
    static const int expected[] = {21, 3, 11, 29, 24, 32, 8, 16, 14, 27, 1, 19,
                                   22, 6, 17, 9, 30, 12, 7, 28, 33, 25, 4, 15,
                                   20, 0, 2, 5, 10, 13, 18, 23, 26, 31, 34};
    check_order(35, expected);
}

static void test_median_of_medians(void) {
    static const int expected[] = {29, 32, 55, 42, 37, 8, 53, 11, 58, 3, 16, 50,
                                   21, 24, 45, 6, 43, 56, 17, 30, 35, 40, 19, 1,
                                   9, 27, 22, 48, 51, 14, 7, 4, 12, 15, 20, 25,
                                   28, 33, 38, 41, 46, 49, 54, 59, 52, 26, 5, 47,
                                   23, 57, 18, 13, 44, 10, 2, 39, 36, 0, 34, 31};
    check_order(60, expected);
}

/* And it is a sort: every size up to 300, deepest first, each index once. */
static void test_sorts(void) {
    static Rec r[300];
    for (int n = 0; n <= 300; n++) {
        int seen[300] = {0};
        for (int i = 0; i < n; i++) {
            r[i].depth = (int)((unsigned)rand() % 50);
            r[i].index = i;
        }
        RetailQsort(r, (size_t)n, sizeof *r, deeper_first);
        for (int i = 0; i < n; i++) {
            if (i > 0)
                ASSERT_TRUE(r[i - 1].depth >= r[i].depth);
            ASSERT_TRUE(r[i].index >= 0 && r[i].index < n && !seen[r[i].index]);
            seen[r[i].index] = 1;
        }
    }
}

int main(void) {
    printf("RetailQsort (LIB386/SYSTEM/QSORT.CPP)\n");
    RUN_TEST(test_insertion_pass);
    RUN_TEST(test_middle_pivot);
    RUN_TEST(test_median_of_three);
    RUN_TEST(test_median_of_medians);
    RUN_TEST(test_sorts);
    TEST_SUMMARY();
    return test_failures ? 1 : 0;
}
