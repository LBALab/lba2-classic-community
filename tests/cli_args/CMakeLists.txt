add_executable(test_cli_args
    test_cli_args.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/CLI_ARGS.CPP
)

target_include_directories(test_cli_args PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
)

set_target_properties(test_cli_args PROPERTIES CXX_STANDARD 11)

add_test(NAME test_cli_args COMMAND test_cli_args)

register_host_test(test_cli_args)
