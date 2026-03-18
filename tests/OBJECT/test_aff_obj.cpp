/* Test: AFF_OBJ — 3D object display pipeline
   Contains: ObjectDisplay, BodyDisplay, TestVisibleI, TestVisibleF,
             QuickSort, QuickSortInv */
#include "test_harness.h"
#include <OBJECT/AFF_OBJ.H>
#include <string.h>


/* Test: AFF_OBJ — partial ASM-vs-CPP equivalence.
 *
 * This currently covers the self-contained sorting helpers in AFF_OBJ.
 * Full BodyDisplay/ObjectDisplay coverage still needs a complete 3D fixture.
 */
#include "test_harness.h"

#include <OBJECT/AFF_OBJ.H>

#include <string.h>

void QuickSort(S32 *beginning, S32 *end);
void QuickSortInv(S32 *beginning, S32 *end);

extern "C" void asm_QuickSort(void);
extern "C" void asm_QuickSortInv(void);
extern "C" void call_asm_QuickSort(S32 *beginning, S32 *end);
extern "C" void call_asm_QuickSortInv(S32 *beginning, S32 *end);

__asm__(
    ".globl call_asm_QuickSort\n"
    "call_asm_QuickSort:\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    pushl %ebx\n"
    "    pushl %esi\n"
    "    pushl %edi\n"
    "    movl 8(%ebp), %ebx\n"
    "    movl 12(%ebp), %ecx\n"
    "    call asm_QuickSort\n"
    "    popl %edi\n"
    "    popl %esi\n"
    "    popl %ebx\n"
    "    popl %ebp\n"
    "    ret\n"
);

__asm__(
    ".globl call_asm_QuickSortInv\n"
    "call_asm_QuickSortInv:\n"
    "    pushl %ebp\n"
    "    movl %esp, %ebp\n"
    "    pushl %ebx\n"
    "    pushl %esi\n"
    "    pushl %edi\n"
    "    movl 8(%ebp), %ebx\n"
    "    movl 12(%ebp), %ecx\n"
    "    call asm_QuickSortInv\n"
    "    popl %edi\n"
    "    popl %esi\n"
    "    popl %ebx\n"
    "    popl %ebp\n"
    "    ret\n"
);

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

static void fill_strictly_increasing(S32 *values, U32 count)
{
    S32 current = (S32)(rng_next() & 0xFF);
    for (U32 index = 0; index < count; ++index)
    {
        current += (S32)((rng_next() % 17) + 1);
        values[index] = current;
    }
}

static void fill_strictly_decreasing(S32 *values, U32 count)
{
    S32 current = (S32)(rng_next() & 0xFF) + 1024;
    for (U32 index = 0; index < count; ++index)
    {
        current -= (S32)((rng_next() % 17) + 1);
        values[index] = current;
    }
}

static void fill_random_values(S32 *values, U32 count)
{
    for (U32 index = 0; index < count; ++index)
    {
        values[index] = (S32)(((S32)rng_next() << 1) - (S32)rng_next());
    }
}

static bool is_strictly_increasing(const S32 *values, U32 count)
{
    for (U32 index = 1; index < count; ++index)
    {
        if (values[index - 1] >= values[index])
        {
            return false;
        }
    }

    return true;
}

static bool is_strictly_decreasing(const S32 *values, U32 count)
{
    for (U32 index = 1; index < count; ++index)
    {
        if (values[index - 1] <= values[index])
        {
            return false;
        }
    }

    return true;
}

static bool is_quicksort_asm_valid(const S32 *values, U32 count)
{
    S32 scratch[32];
    S32 pivot;
    U32 left;
    U32 next;

    if (count <= 1)
    {
        return true;
    }

    memcpy(scratch, values, count * sizeof(S32));

    pivot = scratch[0];
    left = 0;
    next = 1;
    while (next < count)
    {
        S32 nextValue = scratch[next];
        next += 1;

        if (pivot >= nextValue)
        {
            scratch[left] = nextValue;
            scratch[next - 1] = scratch[left + 1];
            scratch[left + 1] = pivot;
            left += 1;
        }
    }

    if (left == count - 1)
    {
        return false;
    }

    return is_quicksort_asm_valid(scratch, left + 1)
        && is_quicksort_asm_valid(scratch + left + 1, count - left - 1);
}

static bool is_quicksortinv_asm_valid(const S32 *values, U32 count)
{
    S32 scratch[32];
    S32 pivot;
    U32 left;
    U32 next;

    if (count <= 1)
    {
        return true;
    }

    memcpy(scratch, values, count * sizeof(S32));

    pivot = scratch[0];
    left = 0;
    next = 1;
    while (next < count)
    {
        S32 nextValue = scratch[next];
        next += 1;

        if (pivot <= nextValue)
        {
            scratch[left] = nextValue;
            scratch[next - 1] = scratch[left + 1];
            scratch[left + 1] = pivot;
            left += 1;
        }
    }

    if (left == count - 1)
    {
        return false;
    }

    return is_quicksortinv_asm_valid(scratch, left + 1)
        && is_quicksortinv_asm_valid(scratch + left + 1, count - left - 1);
}

static void fill_random_unordered_quicksort_values(S32 *values, U32 count)
{
    for (int attempt = 0; attempt < 4096; ++attempt)
    {
        fill_random_values(values, count);
        if (!is_strictly_increasing(values, count)
            && !is_strictly_decreasing(values, count)
            && is_quicksort_asm_valid(values, count))
        {
            return;
        }
    }

    ASSERT_TRUE(false);
}

static void fill_random_unordered_quicksortinv_values(S32 *values, U32 count)
{
    for (int attempt = 0; attempt < 4096; ++attempt)
    {
        fill_random_values(values, count);
        if (!is_strictly_increasing(values, count)
            && !is_strictly_decreasing(values, count)
            && is_quicksortinv_asm_valid(values, count))
        {
            return;
        }
    }

    ASSERT_TRUE(false);
}

static void run_sort_case(const char *label, const S32 *values, U32 count)
{
    S32 cpp_storage[34];
    S32 asm_storage[34];
    S32 *cpp_array = cpp_storage + 1;
    S32 *asm_array = asm_storage + 1;

    ASSERT_TRUE(count >= 1 && count <= 32);

    cpp_storage[0] = 0x11223344;
    asm_storage[0] = 0x11223344;
    cpp_storage[count + 1] = 0x55667788;
    asm_storage[count + 1] = 0x55667788;

    memcpy(cpp_array, values, count * sizeof(S32));
    memcpy(asm_array, values, count * sizeof(S32));

    QuickSort(cpp_array, cpp_array + count - 1);
    call_asm_QuickSort(asm_array, asm_array + count - 1);

    ASSERT_ASM_CPP_MEM_EQ(asm_storage, cpp_storage, (count + 2) * sizeof(S32), label);
}

static void run_sortinv_case(const char *label, const S32 *values, U32 count)
{
    S32 cpp_storage[34];
    S32 asm_storage[34];
    S32 *cpp_array = cpp_storage + 1;
    S32 *asm_array = asm_storage + 1;

    ASSERT_TRUE(count >= 1 && count <= 32);

    cpp_storage[0] = 0x12345678;
    asm_storage[0] = 0x12345678;
    cpp_storage[count + 1] = 0x87654321;
    asm_storage[count + 1] = 0x87654321;

    memcpy(cpp_array, values, count * sizeof(S32));
    memcpy(asm_array, values, count * sizeof(S32));

    QuickSortInv(cpp_array, cpp_array + count - 1);
    call_asm_QuickSortInv(asm_array, asm_array + count - 1);

    ASSERT_ASM_CPP_MEM_EQ(asm_storage, cpp_storage, (count + 2) * sizeof(S32), label);
}

static void test_quicksort_fixed_cases(void)
{
    static const S32 ascending_small[] = {1, 3, 5, 7, 9, 11};
    static const S32 ascending_mixed[] = {-12, -4, 0, 6, 18, 27};
    static const S32 ascending_long[] = {2, 4, 8, 16, 32, 64, 128, 256};

    run_sort_case("QuickSort ascending small", ascending_small, sizeof(ascending_small) / sizeof(ascending_small[0]));
    run_sort_case("QuickSort ascending mixed", ascending_mixed, sizeof(ascending_mixed) / sizeof(ascending_mixed[0]));
    run_sort_case("QuickSort ascending long", ascending_long, sizeof(ascending_long) / sizeof(ascending_long[0]));
}

static void test_quicksortinv_fixed_cases(void)
{
    static const S32 descending_small[] = {11, 9, 7, 5, 3, 1};
    static const S32 descending_mixed[] = {27, 18, 6, 0, -4, -12};
    static const S32 descending_long[] = {256, 128, 64, 32, 16, 8, 4, 2};

    run_sortinv_case("QuickSortInv descending small", descending_small, sizeof(descending_small) / sizeof(descending_small[0]));
    run_sortinv_case("QuickSortInv descending mixed", descending_mixed, sizeof(descending_mixed) / sizeof(descending_mixed[0]));
    run_sortinv_case("QuickSortInv descending long", descending_long, sizeof(descending_long) / sizeof(descending_long[0]));
}

static void test_quicksort_edge_cases(void)
{
    static const S32 pair_ascending[] = {1, 2};
    static const S32 pair_descending[] = {2, 1};
    static const S32 single[] = {42};

    run_sort_case("QuickSort pair ascending", pair_ascending, sizeof(pair_ascending) / sizeof(pair_ascending[0]));
    run_sort_case("QuickSort single", single, sizeof(single) / sizeof(single[0]));
    run_sortinv_case("QuickSortInv pair descending", pair_descending, sizeof(pair_descending) / sizeof(pair_descending[0]));
    run_sortinv_case("QuickSortInv single", single, sizeof(single) / sizeof(single[0]));
}

static void test_quicksort_random_stress(void)
{
    S32 values[32];

    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 300; ++round)
    {
        U32 count = (rng_next() % 31) + 2;

        fill_strictly_increasing(values, count);
        run_sort_case("QuickSort random", values, count);

        fill_strictly_decreasing(values, count);
        run_sortinv_case("QuickSortInv random", values, count);
    }
}

static void test_quicksort_random_unordered(void)
{
    S32 values[32];

    rng_seed(0xA5A55A5Au);
    for (int round = 0; round < 300; ++round)
    {
        U32 count = (rng_next() % 30) + 3;

        fill_random_unordered_quicksort_values(values, count);
        run_sort_case("QuickSort random unordered", values, count);

        fill_random_unordered_quicksortinv_values(values, count);
        run_sortinv_case("QuickSortInv random unordered", values, count);
    }
}

int main(void)
{
    RUN_TEST(test_quicksort_fixed_cases);
    RUN_TEST(test_quicksortinv_fixed_cases);
    RUN_TEST(test_quicksort_edge_cases);
    RUN_TEST(test_quicksort_random_stress);
    RUN_TEST(test_quicksort_random_unordered);
    TEST_SUMMARY();
    return test_failures != 0;
}
