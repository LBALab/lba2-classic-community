# Host coverage for the long double / lrintl rotation path.
#
# The ASM-equivalence tests that own this code (tests/3D, tests/OBJECT,
# tests/ANIM) need the 32-bit UASM toolchain and only run in the Docker leg, so
# the sanitize job in linux.yml never reaches it. This compiles the same sources
# without an ASM twin, which puts them under ASan and UBSan in CI.
add_executable(test_precision_paths
    test_precision_paths.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/3D/LROT2DF.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/SINTABF.CPP
)

# CAMERA.CPP is not linked: it owns the X0/Y0/Z0 the rotation writes to, but
# also pulls in the matrix and projection stack, which would drag most of the 3D
# library into a test that only needs three globals. The test defines them.

target_include_directories(test_precision_paths PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)

target_compile_definitions(test_precision_paths PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)

set_target_properties(test_precision_paths PROPERTIES CXX_STANDARD 11)

add_test(NAME test_precision_paths COMMAND test_precision_paths)

register_host_test(test_precision_paths)
