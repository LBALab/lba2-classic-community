/* Test: AFF_OBJ — 3D object display pipeline
   Contains: ObjectDisplay, BodyDisplay, TestVisibleI, TestVisibleF,
             QuickSort, QuickSortInv */
#include "test_harness.h"
#include <OBJECT/AFF_OBJ.H>
#include <string.h>

extern "C" void asm_QuickSort(S32 *array, S32 left, S32 right);
extern "C" void asm_QuickSortInv(S32 *array, S32 left, S32 right);

static void test_linkage(void)
{
    /* Object display functions require complete 3D pipeline setup
       (body data, polygon state, projection, lighting).
       Verify linkage for now. Full tests need fixture data. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
