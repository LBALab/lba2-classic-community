# Host test for the menu plasma's evolution (SOURCES/PLASMA.CPP). Links that one
# translation unit and nothing else: no assembler, no engine, no retail data, so
# unlike tests/test_plasma.cpp (which compares against PLASMA.ASM and therefore
# lives behind LBA2_BUILD_ASM_EQUIV_TESTS) this one runs in CI on every platform.
add_executable(test_plasma_steps test_plasma_steps.cpp)

target_sources(test_plasma_steps PRIVATE ${CMAKE_SOURCE_DIR}/SOURCES/PLASMA.CPP)

target_include_directories(test_plasma_steps PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES     # PLASMA.H
    ${CMAKE_SOURCE_DIR}/LIB386/H    # SYSTEM/ADELINE_TYPES.H
    ${CMAKE_SOURCE_DIR}/tests       # test_harness.h
)
set_target_properties(test_plasma_steps PROPERTIES CXX_STANDARD 11)

add_test(NAME test_plasma_steps COMMAND test_plasma_steps)
register_host_test(test_plasma_steps)
