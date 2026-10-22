# 3D/X87REAL.H's inline rounding has no 1997 assembly to compare against, and
# the routines that use it are checked against the ASM only on x86, where they
# take the long double branch. This checks the double branch on every host.
add_executable(test_x87round test_x87round.cpp)
target_include_directories(test_x87round PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H ${CMAKE_SOURCE_DIR}/tests)
target_compile_definitions(test_x87round PRIVATE "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\"")
set_target_properties(test_x87round PROPERTIES CXX_STANDARD 11)
add_test(NAME test_x87round COMMAND test_x87round)
register_host_test(test_x87round)
