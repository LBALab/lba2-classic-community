add_executable(test_credits_parse
    test_credits_parse.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/CREDITS_PARSE.CPP
)

target_include_directories(test_credits_parse PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_credits_parse PROPERTIES CXX_STANDARD 11)

add_test(NAME test_credits_parse COMMAND test_credits_parse)

register_host_test(test_credits_parse)
