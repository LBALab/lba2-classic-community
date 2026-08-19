# The recording header: field lookup and the two binding tables it carries. Pure
# string and table work, so the link is the module under test plus INPUT_BINDINGS for
# the retail tables the round trip is checked against. No engine, no SDL runtime, no
# retail data: that is what the extraction out of RECORD.CPP bought.
add_executable(test_record_format
    test_record_format.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/RECORD_FORMAT.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    # INPUT.CPP's two keyboard hooks, stubbed. Shared with tests/input_bindings so
    # there is one stub rather than a second that can drift from it.
    ${CMAKE_SOURCE_DIR}/tests/input_bindings/held_keys.cpp)

target_include_directories(test_record_format PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests/input_bindings
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
)

# KEYBOARD_KEYS.H pulls in <SDL3/SDL_keycode.h>; link SDL3 for its include dirs, as
# tests/input_bindings does.
target_link_libraries(test_record_format PRIVATE SDL3::SDL3)

# LogPrintf routes through the structured log core; bypass the SDL_Log spine.
target_compile_definitions(test_record_format PRIVATE LBA_LOG_NO_SDL)

set_target_properties(test_record_format PROPERTIES CXX_STANDARD 11)

add_test(NAME test_record_format COMMAND test_record_format)
register_host_test(test_record_format)
