add_executable(test_present_rect test_present_rect.cpp)

target_include_directories(test_present_rect PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_link_libraries(test_present_rect PRIVATE SDL3::SDL3)

set_target_properties(test_present_rect PROPERTIES CXX_STANDARD 11)

add_test(NAME test_present_rect COMMAND test_present_rect)

register_host_test(test_present_rect)
