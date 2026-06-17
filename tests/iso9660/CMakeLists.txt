# Host regression test for the ISO9660 reader (LIB386/SYSTEM/ISO9660.CPP).
# Builds a synthetic ISO9660 image in-test and pins that the reader auto-detects
# 2352-raw vs 2048-cooked layouts and resolves a file by path. The reader is
# SDL-free and asset-free, so the test compiles it directly with no SDL, no
# audio device, no disc image beyond the fixture it writes to the build dir.
add_executable(test_iso9660
    test_iso9660.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ISO9660.CPP
)

target_include_directories(test_iso9660 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_compile_definitions(test_iso9660 PRIVATE
    TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/"
    TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

set_target_properties(test_iso9660 PROPERTIES CXX_STANDARD 11)

add_test(NAME test_iso9660 COMMAND test_iso9660)
register_host_test(test_iso9660)
