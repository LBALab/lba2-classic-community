add_executable(test_hqfile test_hqfile.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/HQFILE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LZ.CPP
    # FILES.CPP falls back to the disc-image source on read; pull in its
    # (unexercised here) implementation and the ISO9660 reader it needs so the
    # link resolves.
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FILES.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DISCIMG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ISO9660.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOADSAVE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOGPRINT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP)

target_include_directories(test_hqfile PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

# Log_Warn routes through the structured log core (LOG.CPP); bypass the SDL_Log
# spine so this test needs no SDL.
target_compile_definitions(test_hqfile PRIVATE LBA_LOG_NO_PLATFORM)

set_target_properties(test_hqfile PROPERTIES CXX_STANDARD 11)

add_test(NAME test_hqfile COMMAND test_hqfile)

register_host_test(test_hqfile)
