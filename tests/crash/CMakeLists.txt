# Host test for the crash report's platform-independent half: formatting, modules,
# frames and state fields, captured in memory. No signal is raised.
add_executable(test_crash_core
    test_crash_core.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CRASH.CPP)

target_include_directories(test_crash_core PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests)
target_compile_definitions(test_crash_core PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\"")
set_target_properties(test_crash_core PROPERTIES CXX_STANDARD 11)

add_test(NAME test_crash_core COMMAND test_crash_core)
register_host_test(test_crash_core)

