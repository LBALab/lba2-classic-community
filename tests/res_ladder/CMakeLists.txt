add_executable(test_res_ladder test_res_ladder.cpp)

target_include_directories(test_res_ladder PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_res_ladder PROPERTIES CXX_STANDARD 11)

add_test(NAME test_res_ladder COMMAND test_res_ladder)

register_host_test(test_res_ladder)
