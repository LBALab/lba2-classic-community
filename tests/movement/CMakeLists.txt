# Host regression: character/NPC locomotion must be frame-rate independent (#358).
#
# Drives the real C ObjectSetInterDep over a fixed span of game-time at several
# frame rates and asserts the walked distance matches. CPP-only (no ASM, no
# Docker, no game assets), so it runs in host_quick CI on every platform. The
# anim fixture is shared with the ANIM equivalence tests.
add_executable(test_move_framerate
    test_move_framerate.cpp
)
target_include_directories(test_move_framerate PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/tests/ANIM
    ${CMAKE_BINARY_DIR}
)
target_link_libraries(test_move_framerate PRIVATE ani obj 3D poly svg sys SDL3::SDL3)
set_target_properties(test_move_framerate PROPERTIES CXX_STANDARD 11)
add_test(NAME test_move_framerate COMMAND test_move_framerate)
register_host_test(test_move_framerate)
