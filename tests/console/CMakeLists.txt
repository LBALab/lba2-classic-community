add_executable(test_console_state test_console_state.cpp)
add_executable(test_console_commands test_console_commands.cpp)
add_executable(test_console_give test_console_give.cpp)
add_executable(test_console_render test_console_render.cpp)
add_executable(test_console_complete test_console_complete.cpp)
add_executable(test_console_input test_console_input.cpp)

target_include_directories(test_console_state PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)
target_include_directories(test_console_commands PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)
target_include_directories(test_console_render PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)
target_include_directories(test_console_complete PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)
target_include_directories(test_console_input PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)
target_include_directories(test_console_give PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)
target_compile_definitions(test_console_give PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)

target_link_libraries(test_console_state PRIVATE console)
target_link_libraries(test_console_commands PRIVATE console)
target_link_libraries(test_console_give PRIVATE console)
target_link_libraries(test_console_render PRIVATE console)
target_link_libraries(test_console_complete PRIVATE console)
target_link_libraries(test_console_input PRIVATE console)

set_target_properties(test_console_state PROPERTIES CXX_STANDARD 11)
set_target_properties(test_console_commands PROPERTIES CXX_STANDARD 11)
set_target_properties(test_console_give PROPERTIES CXX_STANDARD 11)
set_target_properties(test_console_render PROPERTIES CXX_STANDARD 11)
set_target_properties(test_console_complete PROPERTIES CXX_STANDARD 11)
set_target_properties(test_console_input PROPERTIES CXX_STANDARD 11)

add_test(NAME test_console_state COMMAND test_console_state)
add_test(NAME test_console_commands COMMAND test_console_commands)
add_test(NAME test_console_give COMMAND test_console_give)
add_test(NAME test_console_render COMMAND test_console_render)
add_test(NAME test_console_complete COMMAND test_console_complete)
add_test(NAME test_console_input COMMAND test_console_input)

register_host_test(test_console_state)
register_host_test(test_console_commands)
register_host_test(test_console_give)
register_host_test(test_console_render)
register_host_test(test_console_complete)
register_host_test(test_console_input)
