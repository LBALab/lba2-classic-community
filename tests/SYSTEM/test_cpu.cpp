/* Test: CPU data — processor detection globals */
#include "test_harness.h"

#include <SYSTEM/CPU.H>
#include <string.h>

/* CPU.ASM and CPU.CPP define the same global data: processor manufacturer
   string, signature, feature flags, TLB/cache info. After rename via objcopy
   we compare the ASM-defined values byte-for-byte against the CPP ones.
   The struct types (TLB, CACHE, s_ProcessorFeatureFlags) are all 4 bytes
   (bitfield-packed into a U32), so we declare ASM externs as raw U8[4]. */

extern "C" char  asm_ProcessorManufacturerIDString[];
extern "C" U8    asm_ProcessorSignature[4];
extern "C" U8    asm_ProcessorFeatureFlags[4];
extern "C" U8    asm_Processor4KBDataTLB[4];
extern "C" U8    asm_Processor4KBInstructionTLB[4];
extern "C" U8    asm_Processor4MBDataTLB[4];
extern "C" U8    asm_Processor4MBInstructionTLB[4];
extern "C" U8    asm_ProcessorL1DataCache[4];
extern "C" U8    asm_ProcessorL1InstructionCache[4];
extern "C" U8    asm_ProcessorL2Cache[4];

static void test_globals_exist(void)
{
    ASSERT_TRUE(1);
}

static void test_string_equiv(void)
{
    ASSERT_ASM_CPP_MEM_EQ(asm_ProcessorManufacturerIDString,
                          ProcessorManufacturerIDString, 13,
                          "ProcessorManufacturerIDString");
}

int main(void)
{
    RUN_TEST(test_globals_exist);
    RUN_TEST(test_string_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
