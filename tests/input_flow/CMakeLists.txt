# The per-boundary input flow counters. The counting rules are pure functions of
# two samples, so this links the module and nothing else: no SDL, no keyboard,
# no engine. Same shape as tests/camera.
add_executable(test_input_flow
    test_input_flow.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP)

target_include_directories(test_input_flow PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_input_flow PROPERTIES CXX_STANDARD 11)

add_test(NAME test_input_flow COMMAND test_input_flow)

register_host_test(test_input_flow)
