# Host regression test for CD audio byte order (LIB386/SYSTEM/CDDA.CPP).
# Pins the rule that decides whether a raw audio track needs its byte pairs
# swapped, which is silent when wrong: the themes play as full-scale noise.
# Pure statistics over a buffer: no SDL, no filesystem, no disc image.
add_executable(test_cdda
    test_cdda.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CDDA.CPP)

target_include_directories(test_cdda PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/LIB386/H)

set_target_properties(test_cdda PROPERTIES CXX_STANDARD 11)

add_test(NAME test_cdda COMMAND test_cdda)
register_host_test(test_cdda)
