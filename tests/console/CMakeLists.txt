add_executable(test_console_state test_console_state.cpp)

target_include_directories(test_console_state PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_link_libraries(test_console_state PRIVATE console)

set_target_properties(test_console_state PROPERTIES CXX_STANDARD 11)

add_test(NAME test_console_state COMMAND test_console_state)
