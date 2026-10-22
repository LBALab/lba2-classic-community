# Host regression test for the CD-track mapping (LIB386/SYSTEM/CDTRACKS.CPP).
# Pins which CD track carries which music on a disc that keeps its soundtrack as
# Red Book audio, and that the mapping stays in step with SOURCES/MUSIC.CPP and
# with the copy scripts/dev/disc_extract.py names its output by.
# Pure table lookup: no SDL, no filesystem, no disc image.
add_executable(test_cdtracks
    test_cdtracks.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CDTRACKS.CPP)

target_include_directories(test_cdtracks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/LIB386/H)

lba2_test_device_data(SOURCES/MUSIC.CPP scripts/dev/disc_extract.py)
target_compile_definitions(test_cdtracks PRIVATE TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/"
    TEST_DATA_ROOT="${LBA2_TEST_DATA_ROOT}/")

set_target_properties(test_cdtracks PROPERTIES CXX_STANDARD 11)

add_test(NAME test_cdtracks COMMAND test_cdtracks)
register_host_test(test_cdtracks)
