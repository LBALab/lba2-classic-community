add_executable(test_res_catalog
    test_res_catalog.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/RES_CATALOG.CPP
)

target_include_directories(test_res_catalog PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_res_catalog PROPERTIES CXX_STANDARD 11)

add_test(NAME test_res_catalog COMMAND test_res_catalog)

register_host_test(test_res_catalog)
