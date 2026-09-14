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

# The process-level test: a child that links the real handler and crashes, run once
# per way of dying. Only where a backend exists.
if(APPLE OR (UNIX AND NOT ANDROID) OR (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8))
    add_executable(crash_child
        crash_child.cpp
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CRASH.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CRASH_POSIX.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CRASH_LINUX.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CRASH_MACOS.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CRASH_WIN.CPP)
    target_include_directories(crash_child PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    set_target_properties(crash_child PROPERTIES CXX_STANDARD 11)
    find_package(Threads REQUIRED)
    target_link_libraries(crash_child PRIVATE Threads::Threads)

    add_executable(test_crash_report test_crash_report.cpp)
    target_include_directories(test_crash_report PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_compile_definitions(test_crash_report PRIVATE
        "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\"")
    set_target_properties(test_crash_report PROPERTIES CXX_STANDARD 11)
    add_dependencies(test_crash_report crash_child)

    add_test(NAME test_crash_report COMMAND test_crash_report $<TARGET_FILE:crash_child>)
    register_host_test(test_crash_report)
endif()
