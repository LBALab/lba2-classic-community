add_executable(test_copymask_negx
    test_copymask_negx.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SVGA/COPYMASK.CPP
)

target_include_directories(test_copymask_negx PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_copymask_negx PROPERTIES CXX_STANDARD 11)

add_test(NAME test_copymask_negx COMMAND test_copymask_negx)

register_host_test(test_copymask_negx)
