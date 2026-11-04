add_executable(test_gl_triangulate
    test_gl_triangulate.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/GL/TRIANGULATE.CPP)

target_include_directories(test_gl_triangulate PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H)

set_target_properties(test_gl_triangulate PROPERTIES CXX_STANDARD 11)

add_test(NAME test_gl_triangulate COMMAND test_gl_triangulate)
register_host_test(test_gl_triangulate)
