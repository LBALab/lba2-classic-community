add_executable(test_muldiv64
    test_muldiv64.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/SYSTEM.CPP
)

target_include_directories(test_muldiv64 PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)

set_target_properties(test_muldiv64 PROPERTIES CXX_STANDARD 11)

add_test(NAME test_muldiv64 COMMAND test_muldiv64)

register_host_test(test_muldiv64)
