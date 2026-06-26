add_executable(test_atomic_write test_atomic_write.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FILES.CPP
    # FILES.CPP falls back to the disc-image source on read; pull in its
    # (unexercised here) implementation and the ISO9660 reader it needs so the
    # link resolves.
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DISCIMG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ISO9660.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOADSAVE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOGPRINT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP)

target_include_directories(test_atomic_write PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

# LogPrintf routes through the structured log core (LOG.CPP); bypass the SDL_Log
# spine so this test needs no SDL.
target_compile_definitions(test_atomic_write PRIVATE LBA_LOG_NO_SDL)

set_target_properties(test_atomic_write PROPERTIES CXX_STANDARD 11)

add_test(NAME test_atomic_write COMMAND test_atomic_write)

register_host_test(test_atomic_write)
