# Host regression test for the CUE-sheet reader (LIB386/SYSTEM/CUE.CPP).
# Pins external-audio extraction (GOG's LBA2.OGG) and the skip rules (data-only,
# in-image CD-DA under a BINARY file). The parser is pure text, so the test
# compiles it directly: no SDL, no filesystem, no disc image.
add_executable(test_cue
    test_cue.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CUE.CPP)

target_include_directories(test_cue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/LIB386/H)

target_compile_definitions(test_cue PRIVATE TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/")

set_target_properties(test_cue PROPERTIES CXX_STANDARD 11)

add_test(NAME test_cue COMMAND test_cue)
register_host_test(test_cue)
