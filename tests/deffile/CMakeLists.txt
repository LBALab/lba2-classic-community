add_executable(test_deffile_latin1 test_deffile_latin1.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DEFFILE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FILES.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOADSAVE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOGPRINT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ITOA.CPP)

target_include_directories(test_deffile_latin1 PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_deffile_latin1 PROPERTIES CXX_STANDARD 11)

add_test(NAME test_deffile_latin1 COMMAND test_deffile_latin1)

register_host_test(test_deffile_latin1)
