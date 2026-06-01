add_executable(test_deffile_latin1 test_deffile_latin1.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DEFFILE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FILES.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOADSAVE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOGPRINT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ITOA.CPP)

target_include_directories(test_deffile_latin1 PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

# LogPrintf now routes through the structured log core (LOG.CPP); pull it in and
# bypass the SDL_Log spine so this test needs no SDL.
target_compile_definitions(test_deffile_latin1 PRIVATE LBA_LOG_NO_SDL)

set_target_properties(test_deffile_latin1 PROPERTIES CXX_STANDARD 11)

add_test(NAME test_deffile_latin1 COMMAND test_deffile_latin1)

register_host_test(test_deffile_latin1)
