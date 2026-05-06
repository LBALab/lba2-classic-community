# Host-only check that the version-string generation produces matching
# outputs (build/VERSION.txt text file vs LBA2_VERSION_STRING macro in
# build/VERSION_GENERATED.h). Pure CMake-level test — no compiler, no
# binary launch. Runs everywhere CMake does.
add_test(NAME test_version_drift
    COMMAND ${CMAKE_COMMAND}
            -DVERSION_FILE=${CMAKE_BINARY_DIR}/VERSION.txt
            -DHEADER_FILE=${CMAKE_BINARY_DIR}/VERSION_GENERATED.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_version.cmake)

# The test depends on the version-gen target's outputs existing; building
# host_tests already covers that, but make ctest report a useful failure
# instead of a silent missing-file error if someone runs ctest before any
# build step.
set_tests_properties(test_version_drift PROPERTIES
    LABELS host_quick
    REQUIRED_FILES "${CMAKE_BINARY_DIR}/VERSION.txt;${CMAKE_BINARY_DIR}/VERSION_GENERATED.h")
