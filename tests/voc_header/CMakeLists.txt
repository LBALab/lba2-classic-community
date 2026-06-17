# Host regression test for the VOC header parser (LIB386/AIL/SDL/VOC_HEADER.CPP).
# Pins that detection uses the 19-byte offset-1 signature, not the first byte, so VOC banks
# whose first byte is 'C' (standard), 0x00 or 0x01 (LBA voice banks) all parse. The parser is
# pure and SDL-free, so the test compiles it directly with no SDL, no audio device, no assets.
add_executable(test_voc_header
    test_voc_header.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/VOC_HEADER.CPP
)

target_include_directories(test_voc_header PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_compile_definitions(test_voc_header PRIVATE TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/")

set_target_properties(test_voc_header PROPERTIES CXX_STANDARD 11)

add_test(NAME test_voc_header COMMAND test_voc_header)
register_host_test(test_voc_header)
