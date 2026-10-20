add_executable(test_mouse_capture
    test_mouse_capture.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/MOUSE_CAPTURE.CPP)

target_include_directories(test_mouse_capture PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

add_test(NAME test_mouse_capture COMMAND test_mouse_capture)

register_host_test(test_mouse_capture)
