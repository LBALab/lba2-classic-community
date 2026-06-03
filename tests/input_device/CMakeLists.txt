add_executable(test_input_device
    test_input_device.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/KEYBOARD.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/MOUSE.CPP)

target_include_directories(test_input_device PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_link_libraries(test_input_device PRIVATE SDL3::SDL3)

add_test(NAME test_input_device COMMAND test_input_device)

register_host_test(test_input_device)
