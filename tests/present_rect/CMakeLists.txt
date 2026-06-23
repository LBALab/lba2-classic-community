add_executable(test_present_rect test_present_rect.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/PRESENT_RECT.CPP)

target_include_directories(test_present_rect PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_present_rect PROPERTIES CXX_STANDARD 11)

add_test(NAME test_present_rect COMMAND test_present_rect)

register_host_test(test_present_rect)
