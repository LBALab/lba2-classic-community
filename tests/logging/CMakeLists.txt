add_executable(test_log
    test_log.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOGPRINT.CPP)

target_include_directories(test_log PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests)

# Test the core without backend diagnostics or a platform thread gate.
target_compile_definitions(test_log PRIVATE LBA_LOG_NO_PLATFORM)

set_target_properties(test_log PROPERTIES CXX_STANDARD 11)

add_test(NAME test_log COMMAND test_log)
register_host_test(test_log)

# Companion target exercises the selected platform's diagnostic transport.
if(PLATFORM_BACKEND MATCHES "^sdl[23]$")
add_executable(test_log_spine
    test_log_spine.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP)

target_include_directories(test_log_spine PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests)

target_link_libraries(test_log_spine PRIVATE lba2_platform ${LBA2_SDL_TARGET})
if(PLATFORM_BACKEND STREQUAL "sdl2")
    target_compile_definitions(test_log_spine PRIVATE LBA2_USE_SDL2=1)
endif()

set_target_properties(test_log_spine PROPERTIES CXX_STANDARD 11)

add_test(NAME test_log_spine COMMAND test_log_spine)
register_host_test(test_log_spine)
endif()
