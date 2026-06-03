add_executable(test_staging_buffer
    test_staging_buffer.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SVGA/STAGING_BUFFER.CPP)

target_include_directories(test_staging_buffer PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H)

set_target_properties(test_staging_buffer PROPERTIES CXX_STANDARD 11)

add_test(NAME test_staging_buffer COMMAND test_staging_buffer)
register_host_test(test_staging_buffer)
