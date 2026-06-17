# Host regression test for the disc-image resource source (LIB386/SYSTEM/DISCIMG.CPP).
# Builds a synthetic nested ISO9660 image in-test and pins mount/asset-root
# detection, generic path resolution, streamed reads, and existence checks. The
# manager is SDL-free, so the test compiles it directly with the ISO9660 reader,
# the FILES seam (for IsDirectory), and the log core (LBA_LOG_NO_SDL): no SDL,
# no audio device, no disc image beyond the fixture it writes to the build dir.
add_executable(test_disc_image
    test_disc_image.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DISCIMG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ISO9660.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FILES.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP)

target_include_directories(test_disc_image PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/LIB386/H)

# Log core bypasses SDL (dispatches straight to sinks), so no SDL is needed.
target_compile_definitions(test_disc_image PRIVATE
    LBA_LOG_NO_SDL
    TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/"
    TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}")

set_target_properties(test_disc_image PROPERTIES CXX_STANDARD 11)

add_test(NAME test_disc_image COMMAND test_disc_image)
register_host_test(test_disc_image)
