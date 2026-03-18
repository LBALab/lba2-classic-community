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
#include <3D/DATAMAT.H>

#include <string.h>

static const U32 kObjProjectedPointCount = 550;

typedef struct
{
    U16 P1;
    U16 P2;
    U16 P3;
    U16 Padding;
    U16 Couleur;
    U16 Normale;
} STRUC_POLY3_LIGHT;

typedef struct
{
    U16 P1;
    U16 P2;
    U16 P3;
    U16 HandleEnv;
    U16 Couleur;
    U16 Normale;
    U16 Scale;
    U16 Padding;
} STRUC_POLY3_ENV;

void QuickSort(S32 *beginning, S32 *end);
void QuickSortInv(S32 *beginning, S32 *end);
bool TestVisible(STRUC_POLY3_ENV *poly);

extern TYPE_PT Obj_ListProjectedPoints[];
extern "C" TYPE_PT asm_Obj_ListProjectedPoints[];

extern "C" int CallTestVisibleI(STRUC_POLY3_LIGHT *poly);
extern "C" void CallQuickSort(S32 *beginning, S32 *end);
extern "C" void CallQuickSortInv(S32 *beginning, S32 *end);

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

static void set_point(TYPE_PT *point, S16 x, S16 y)
{
    point->X = x;
    point->Y = y;
}

static void set_hidden_point(TYPE_PT *point)
{
    point->X = (S16)-0x8000;
    point->Y = (S16)-0x8000;
}

static void build_projected_points_snapshot(TYPE_PT *snapshot, const TYPE_PT *points, U32 count)
{
    ASSERT_TRUE(count <= kObjProjectedPointCount);

    for (U32 index = 0; index < kObjProjectedPointCount; ++index)
    {
        set_hidden_point(&snapshot[index]);
    }

    memcpy(snapshot, points, count * sizeof(TYPE_PT));
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
    /* The original ASM quicksort re-enters the same range for some pivot
       orders, so unordered coverage must stay within the ASM-valid domain. */
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
    /* Same restriction as QuickSort: keep inputs unordered, but reject
       pivot layouts that make the original ASM recurse forever. */
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
    CallQuickSort(asm_array, asm_array + count - 1);

    ASSERT_ASM_CPP_MEM_EQ(asm_storage, cpp_storage, (count + 2) * sizeof(S32), label);
}

static void run_testvisiblei_case(const char *label, const TYPE_PT *points, U32 count, const STRUC_POLY3_LIGHT *poly)
{
    TYPE_PT baseline[kObjProjectedPointCount];

    build_projected_points_snapshot(baseline, points, count);

    memcpy(Obj_ListProjectedPoints, baseline, sizeof(baseline));
    const int cpp_visible = TestVisible((STRUC_POLY3_ENV *)poly) ? 1 : 0;
    ASSERT_MEM_EQ(baseline, Obj_ListProjectedPoints, sizeof(baseline));

    memcpy(asm_Obj_ListProjectedPoints, baseline, sizeof(baseline));
    const int asm_visible = CallTestVisibleI((STRUC_POLY3_LIGHT *)poly);
    ASSERT_MEM_EQ(baseline, asm_Obj_ListProjectedPoints, sizeof(baseline));

    ASSERT_ASM_CPP_EQ_INT(asm_visible, cpp_visible, label);
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
    CallQuickSortInv(asm_array, asm_array + count - 1);

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

static void test_testvisible_fixed_cases(void)
{
    TYPE_PT points[4];
    STRUC_POLY3_LIGHT light_visible = {0, 1, 2, 0, 0x12, 0x34};
    STRUC_POLY3_LIGHT light_hidden = {0, 2, 1, 0, 0x56, 0x78};

    set_point(&points[0], 0, 0);
    set_point(&points[1], 0, 10);
    set_point(&points[2], 10, 0);
    set_point(&points[3], 5, 5);

    run_testvisiblei_case("TestVisibleI visible winding", points, 4, &light_visible);
    run_testvisiblei_case("TestVisibleI hidden winding", points, 4, &light_hidden);
}

static void test_testvisible_edge_cases(void)
{
    TYPE_PT points[4];
    STRUC_POLY3_LIGHT light_invalid = {0, 1, 2, 0, 1, 2};
    STRUC_POLY3_LIGHT light_collinear = {0, 1, 2, 0, 3, 4};

    set_hidden_point(&points[0]);
    set_point(&points[1], 6, -2);
    set_point(&points[2], -4, 9);
    set_point(&points[3], 3, 3);
    run_testvisiblei_case("TestVisibleI invalid hidden point", points, 4, &light_invalid);

    set_point(&points[0], -8, -8);
    set_point(&points[1], 0, 0);
    set_point(&points[2], 8, 8);
    set_point(&points[3], 12, -4);
    run_testvisiblei_case("TestVisibleI collinear", points, 4, &light_collinear);
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

static void test_testvisible_random_stress(void)
{
    TYPE_PT points[8];

    rng_seed(0x13572468u);
    for (int round = 0; round < 300; ++round)
    {
        STRUC_POLY3_LIGHT light_poly;

        for (int index = 0; index < 8; ++index)
        {
            if ((rng_next() % 7u) == 0)
            {
                set_hidden_point(&points[index]);
            }
            else
            {
                set_point(&points[index],
                          (S16)((S32)(rng_next() % 401u) - 200),
                          (S16)((S32)(rng_next() % 401u) - 200));
            }
        }

        light_poly.P1 = (U16)(rng_next() % 8u);
        light_poly.P2 = (U16)(rng_next() % 8u);
        light_poly.P3 = (U16)(rng_next() % 8u);
        light_poly.Padding = 0;
        light_poly.Couleur = (U16)rng_next();
        light_poly.Normale = (U16)rng_next();

        run_testvisiblei_case("TestVisibleI random", points, 8, &light_poly);
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
    RUN_TEST(test_testvisible_fixed_cases);
    RUN_TEST(test_testvisible_edge_cases);
    RUN_TEST(test_testvisible_random_stress);
    RUN_TEST(test_quicksort_fixed_cases);
    RUN_TEST(test_quicksortinv_fixed_cases);
    RUN_TEST(test_quicksort_edge_cases);
    RUN_TEST(test_quicksort_random_stress);
    RUN_TEST(test_quicksort_random_unordered);
    TEST_SUMMARY();
    return test_failures != 0;
}
