# INV64 (LIB386/pol_work/POLY.CPP) had no assembly file to compare against: the
# original was an inline pragma, so it gets a host test instead.
add_executable(test_inv64 test_inv64.cpp)
target_include_directories(test_inv64 PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
target_link_libraries(test_inv64 PRIVATE poly svg sys)
set_target_properties(test_inv64 PROPERTIES CXX_STANDARD 11)
add_test(NAME test_inv64 COMMAND test_inv64)
register_host_test(test_inv64)
