add_executable(test_getascii
    test_getascii.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/KEYBOARD.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP)

target_include_directories(test_getascii PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_link_libraries(test_getascii PRIVATE SDL3::SDL3)

# The project default is C++98; the sibling input tests pin this for the same reason.
set_target_properties(test_getascii PROPERTIES CXX_STANDARD 11)

add_test(NAME test_getascii COMMAND test_getascii)

# 77 is a skip, not a failure. The shell suite's runner maps it by hand
# (tests/automation/run.sh); CTest has to be told, or a run on a host with no SDL
# event subsystem or an exotic keymap turns a deliberate skip into a red job.
set_tests_properties(test_getascii PROPERTIES SKIP_RETURN_CODE 77)

register_host_test(test_getascii)
