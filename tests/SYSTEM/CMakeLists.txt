set(_SYS_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM)
set(_SYS_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

add_asm_cpp_test(
    NAME test_lz  TEST_SOURCE test_lz.cpp
    ASM_SOURCE ${_SYS_ASM_DIR}/LZ.ASM
    RENAME_SYMS ExpandLZ
    LIBS sys
    ASM_INCLUDE_DIRS ${_SYS_ASM_INC}
)

# .model SMALL ASM files are automatically patched to FLAT at build time.
add_asm_cpp_test(
    NAME test_fastcpyf  TEST_SOURCE test_fastcpyf.cpp
    ASM_SOURCE ${_SYS_ASM_DIR}/FASTCPYF.ASM
    RENAME_SYMS FastCopy FastCopyF
    LIBS sys
    ASM_INCLUDE_DIRS ${_SYS_ASM_INC}
)

add_asm_cpp_test(
    NAME test_cpu  TEST_SOURCE test_cpu.cpp
    ASM_SOURCE ${_SYS_ASM_DIR}/CPU.ASM
    RENAME_SYMS ProcessorManufacturerIDString
                ProcessorSignature ProcessorFeatureFlags
                Processor4KBDataTLB Processor4KBInstructionTLB
                Processor4MBDataTLB Processor4MBInstructionTLB
                ProcessorL1DataCache ProcessorL1InstructionCache
                ProcessorL2Cache
    LIBS sys
    ASM_INCLUDE_DIRS ${_SYS_ASM_INC}
)

add_asm_cpp_test(
    NAME test_mousedat  TEST_SOURCE test_mousedat.cpp
    ASM_SOURCE ${_SYS_ASM_DIR}/MOUSEDAT.ASM
    RENAME_SYMS BinGphMouse
    LIBS sys
    ASM_INCLUDE_DIRS ${_SYS_ASM_INC}
)
