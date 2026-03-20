/* Test: CPU data — processor detection globals */
#include "test_harness.h"

#include <SYSTEM/CPU.H>
#include <string.h>

/* CPU.ASM and CPU.CPP define the same global data: processor manufacturer
   string, signature, feature flags, TLB/cache info. After rename via objcopy
   we compare the ASM-defined values byte-for-byte against the CPP ones.
   The struct types (TLB, CACHE, s_ProcessorFeatureFlags) are all 4 bytes
   (bitfield-packed into a U32), so we declare ASM externs as raw U8[4]. */

extern "C" char asm_ProcessorManufacturerIDString[];
extern "C" U8 asm_ProcessorSignature[4];
extern "C" U8 asm_ProcessorFeatureFlags[4];
extern "C" U8 asm_Processor4KBDataTLB[4];
extern "C" U8 asm_Processor4KBInstructionTLB[4];
extern "C" U8 asm_Processor4MBDataTLB[4];
extern "C" U8 asm_Processor4MBInstructionTLB[4];
extern "C" U8 asm_ProcessorL1DataCache[4];
extern "C" U8 asm_ProcessorL1InstructionCache[4];
extern "C" U8 asm_ProcessorL2Cache[4];

static void assert_global_equiv(const void *asm_data, const void *cpp_data,
                                size_t size, const char *label) {
    ASSERT_ASM_CPP_MEM_EQ(asm_data, cpp_data, size, label);
}

static void test_global_initializers_equiv(void) {
    static const U8 expected_signature[4] = {0x00, 0x04, 0x00, 0x00};

    assert_global_equiv(asm_ProcessorManufacturerIDString,
                        ProcessorManufacturerIDString, 13,
                        "ProcessorManufacturerIDString");
    assert_global_equiv(asm_ProcessorSignature, &ProcessorSignature,
                        sizeof(ProcessorSignature), "ProcessorSignature");
    assert_global_equiv(asm_ProcessorFeatureFlags, &ProcessorFeatureFlags,
                        sizeof(ProcessorFeatureFlags), "ProcessorFeatureFlags");
    assert_global_equiv(asm_Processor4KBDataTLB, &Processor4KBDataTLB,
                        sizeof(Processor4KBDataTLB), "Processor4KBDataTLB");
    assert_global_equiv(asm_Processor4KBInstructionTLB, &Processor4KBInstructionTLB,
                        sizeof(Processor4KBInstructionTLB), "Processor4KBInstructionTLB");
    assert_global_equiv(asm_Processor4MBDataTLB, &Processor4MBDataTLB,
                        sizeof(Processor4MBDataTLB), "Processor4MBDataTLB");
    assert_global_equiv(asm_Processor4MBInstructionTLB, &Processor4MBInstructionTLB,
                        sizeof(Processor4MBInstructionTLB), "Processor4MBInstructionTLB");
    assert_global_equiv(asm_ProcessorL1DataCache, &ProcessorL1DataCache,
                        sizeof(ProcessorL1DataCache), "ProcessorL1DataCache");
    assert_global_equiv(asm_ProcessorL1InstructionCache, &ProcessorL1InstructionCache,
                        sizeof(ProcessorL1InstructionCache), "ProcessorL1InstructionCache");
    assert_global_equiv(asm_ProcessorL2Cache, &ProcessorL2Cache,
                        sizeof(ProcessorL2Cache), "ProcessorL2Cache");

    ASSERT_MEM_EQ(expected_signature, (const U8 *)&ProcessorSignature,
                  sizeof(expected_signature));
}

int main(void) {
    RUN_TEST(test_global_initializers_equiv);
    TEST_SUMMARY();
    return test_failures != 0;
}
