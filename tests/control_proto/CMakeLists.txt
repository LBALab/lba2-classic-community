add_executable(test_control_proto
    test_control_proto.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/CONTROL_PROTO.CPP
)

target_include_directories(test_control_proto PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
)

set_target_properties(test_control_proto PROPERTIES CXX_STANDARD 11)

add_test(NAME test_control_proto COMMAND test_control_proto)

register_host_test(test_control_proto)
