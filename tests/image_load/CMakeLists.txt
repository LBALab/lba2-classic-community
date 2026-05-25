add_executable(test_image_load
    test_image_load.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/IMAGE_LOAD.CPP
)

target_include_directories(test_image_load PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_image_load PROPERTIES CXX_STANDARD 11)

add_test(NAME test_image_load COMMAND test_image_load)

register_host_test(test_image_load)
