/* Test: SinTab/CosTab - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/SINTAB.H>

extern "C" S16 asm_SinTab[];
extern "C" S16 asm_CosTab[];

static void test_equivalence(void) {
    for (int i = 0; i < 4096; i++)
        ASSERT_ASM_CPP_EQ_INT(asm_SinTab[i], SinTab[i], "SinTab");
    for (int i = 0; i < 4096; i++)
        ASSERT_ASM_CPP_EQ_INT(asm_CosTab[i], CosTab[i], "CosTab");
}

/* The original ASM lays out SinTab (1024 entries) and CosTab (4096 entries)
   as one contiguous 5120-entry block.  Game code indexes SinTab[n] with n up
   to 4095 (e.g. SinTab[CameraBeta] in SORT.CPP), relying on the overflow
   reading into CosTab.  Verify the CPP arrays honour this contract by
   checking that CosTab points exactly to &SinTab[1024]. */
static void test_contiguity(void) {
    if (CosTab != &SinTab[1024]) {
        fprintf(stderr, "FAIL: CosTab = %p, &SinTab[1024] = %p (must be equal)\n",
                (void *)CosTab, (void *)&SinTab[1024]);
        test_failures++;
    }
    if (&SinTab[1024] != &CosTab[0]) {
        fprintf(stderr, "FAIL: &SinTab[1024] = %p, &CosTab[0] = %p (must be equal)\n"
                        "  SinTab and CosTab must be contiguous; game code indexes\n"
                        "  SinTab[n] for n > 1023, reading into CosTab memory.\n",
                (void *)&SinTab[1024], (void *)&CosTab[0]);
        test_failures++;
    }
}

int main(void) {
    RUN_TEST(test_equivalence);
    RUN_TEST(test_contiguity);
    TEST_SUMMARY();
    return test_failures != 0;
}
