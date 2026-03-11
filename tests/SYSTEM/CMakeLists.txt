set(_SYS_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM)
set(_SYS_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

# LZ.ASM uses .model FLAT — ASM equivalence test works
add_asm_cpp_test(
    NAME test_lz  TEST_SOURCE test_lz.cpp
    ASM_SOURCE ${_SYS_ASM_DIR}/LZ.ASM
    RENAME_SYMS ExpandLZ
    LIBS sys
    ASM_INCLUDE_DIRS ${_SYS_ASM_INC}
)

# FASTCPYF.ASM uses .model SMALL → CPP-only
add_cpp_test(NAME test_fastcpyf  TEST_SOURCE test_fastcpyf.cpp  LIBS sys)

# CPU.ASM uses .model FLAT — ASM equivalence test works
add_asm_cpp_test(
    NAME test_cpu  TEST_SOURCE test_cpu.cpp
    ASM_SOURCE ${_SYS_ASM_DIR}/CPU.ASM
    RENAME_SYMS ProcessorManufacturerIDString
    LIBS sys
    ASM_INCLUDE_DIRS ${_SYS_ASM_INC}
)

# MOUSEDAT.ASM uses .model SMALL → CPP-only
add_cpp_test(NAME test_mousedat  TEST_SOURCE test_mousedat.cpp  LIBS sys)
