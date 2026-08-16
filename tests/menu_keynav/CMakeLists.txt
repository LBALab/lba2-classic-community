add_executable(test_menu_keynav
    test_menu_keynav.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/MENU_KEYNAV.CPP)

target_include_directories(test_menu_keynav PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
)

# KEYBOARD_KEYS.H pulls in <SDL3/SDL_keycode.h>; link SDL3 for its include dirs
# (the mapping itself calls no SDL). Matches tests/input_funnel.
target_link_libraries(test_menu_keynav PRIVATE SDL3::SDL3)

add_test(NAME test_menu_keynav COMMAND test_menu_keynav)

register_host_test(test_menu_keynav)
