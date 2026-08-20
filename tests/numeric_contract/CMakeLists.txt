# The arithmetic a recording is replayed against: LIB386/3D's long double paths.
#
# Links the maths translation units and nothing else. The camera globals they
# write to live in CAMERA.CPP, which would pull the whole matrix and projection
# stack in behind it, so the test defines the dozen scalars itself the way
# tests/precision_paths does. The projection recorder is stubbed there too: it is
# a capture hook, not arithmetic.
#
# No retail data and no SDL, so this runs in CI on every platform, which is the
# point. On ARM it reports rather than asserts, and the macOS job log is where
# the ARM numbers come from.
add_executable(test_numeric_contract
    test_numeric_contract.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/3D/DISTANCE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/LPROJ3DF.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/LROT3DF.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/LIROT3DF.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/LROT2DF.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/SINTABF.CPP
)

target_include_directories(test_numeric_contract PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)

target_compile_definitions(test_numeric_contract PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)

set_target_properties(test_numeric_contract PROPERTIES CXX_STANDARD 11)

add_test(NAME test_numeric_contract COMMAND test_numeric_contract)

register_host_test(test_numeric_contract)
