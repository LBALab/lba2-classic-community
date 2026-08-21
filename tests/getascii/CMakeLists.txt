add_executable(test_getascii
    test_getascii.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/KEYBOARD.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP)

target_include_directories(test_getascii PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_link_libraries(test_getascii PRIVATE SDL3::SDL3)

add_test(NAME test_getascii COMMAND test_getascii)

register_host_test(test_getascii)
