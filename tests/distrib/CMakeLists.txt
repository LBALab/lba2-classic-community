add_executable(test_distrib_resolve test_distrib_resolve.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/DISTRIB.CPP)

target_include_directories(test_distrib_resolve PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_distrib_resolve PROPERTIES CXX_STANDARD 11)

add_test(NAME test_distrib_resolve COMMAND test_distrib_resolve)

register_host_test(test_distrib_resolve)
