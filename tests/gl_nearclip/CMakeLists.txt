add_executable(test_gl_nearclip
    test_gl_nearclip.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/GL/NEARCLIP.CPP)

target_include_directories(test_gl_nearclip PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H)

set_target_properties(test_gl_nearclip PROPERTIES CXX_STANDARD 11)

add_test(NAME test_gl_nearclip COMMAND test_gl_nearclip)
register_host_test(test_gl_nearclip)
