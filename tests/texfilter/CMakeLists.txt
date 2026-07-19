# Blend tables for the optional texture filter: pure data in, data out, with no
# ASM counterpart, so it belongs to the host suite rather than the equivalence
# one.
add_executable(test_polyfilt
    test_polyfilt.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/pol_work/POLYFILT.CPP
)

target_include_directories(test_polyfilt PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_polyfilt PROPERTIES CXX_STANDARD 11)

add_test(NAME test_polyfilt COMMAND test_polyfilt)

register_host_test(test_polyfilt)
