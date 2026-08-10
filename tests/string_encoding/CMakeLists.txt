# Host test for the mixed-encoding string comparison used to read language
# names out of a config file. Pulls STRING.CPP in directly (it needs SDL only
# for SDL_tolower/SDL_strlen); no engine, no device.
add_executable(test_string_encoding
    test_string_encoding.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/STRING.CPP
)

target_include_directories(test_string_encoding PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_link_libraries(test_string_encoding PRIVATE SDL3::SDL3)

set_target_properties(test_string_encoding PROPERTIES CXX_STANDARD 11)

add_test(NAME test_string_encoding COMMAND test_string_encoding)

register_host_test(test_string_encoding)
