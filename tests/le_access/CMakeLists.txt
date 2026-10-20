add_executable(test_le_access
    test_le_access.cpp
)

target_include_directories(test_le_access PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)

add_test(NAME test_le_access COMMAND test_le_access)

register_host_test(test_le_access)
