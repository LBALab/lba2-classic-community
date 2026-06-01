add_executable(test_log
    test_log.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOGPRINT.CPP)

target_include_directories(test_log PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests)

# Bypass the SDL_Log spine: the core dispatches straight to the sinks, so the
# host test needs no SDL.
target_compile_definitions(test_log PRIVATE LBA_LOG_NO_SDL)

set_target_properties(test_log PROPERTIES CXX_STANDARD 11)

add_test(NAME test_log COMMAND test_log)
register_host_test(test_log)

# Companion target that exercises the REAL SDL_Log spine (no LBA_LOG_NO_SDL):
# Log_* -> SDL_LogMessage -> output function -> log_emit -> sink. Links SDL3.
add_executable(test_log_spine
    test_log_spine.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP)

target_include_directories(test_log_spine PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests)

target_link_libraries(test_log_spine PRIVATE SDL3::SDL3)

set_target_properties(test_log_spine PROPERTIES CXX_STANDARD 11)

add_test(NAME test_log_spine COMMAND test_log_spine)
register_host_test(test_log_spine)
