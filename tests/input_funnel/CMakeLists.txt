add_executable(test_input_funnel
    test_input_funnel.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP)

target_include_directories(test_input_funnel PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
)

# KEYBOARD_KEYS.H pulls in <SDL3/SDL_keycode.h>; link SDL3 for its include dirs
# (the funnel itself calls no SDL). Matches tests/input_device.
target_link_libraries(test_input_funnel PRIVATE SDL3::SDL3)

add_test(NAME test_input_funnel COMMAND test_input_funnel)

register_host_test(test_input_funnel)
