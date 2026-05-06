# Wires a build-time + configure-time version-string generation step.
#
# Produces two artifacts on every `cmake --build`:
#
#   ${CMAKE_BINARY_DIR}/VERSION.txt              — plain text, single line.
#                                                  Release pipelines (PR #74)
#                                                  read this to name artifacts.
#   ${CMAKE_BINARY_DIR}/VERSION_GENERATED.h      — defines LBA2_VERSION_STRING
#                                                  (included by BUILD_INFO.h).
#
# Source of truth is the `VERSION` file at the repo root. See
# `cmake/write_version.cmake` for the resolution logic.
#
# Note on the .txt extension on the plain-text output: the build dir is
# on lba2's include path (BUILD_INFO.h is generated there). On macOS
# (default case-insensitive APFS/HFS+), a C++ header search for
# `<version>` (a real libc++ feature-test header since C++20) would
# otherwise resolve to `${CMAKE_BINARY_DIR}/VERSION` and clang would try
# to compile the plain-text file as C++. The `.txt` extension makes the
# collision impossible.

find_package(Git QUIET)

set(_VERSION_TXT "${CMAKE_BINARY_DIR}/VERSION.txt")
set(_VERSION_HDR "${CMAKE_BINARY_DIR}/VERSION_GENERATED.h")
set(_VERSION_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/write_version.cmake")

# Build-time step: regenerate on every build (cheap; write-on-change inside
# the script avoids retriggering downstream rebuilds when nothing moves).
add_custom_command(
    OUTPUT ${_VERSION_TXT} ${_VERSION_HDR}
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUT_TXT=${_VERSION_TXT}
            -DOUT_HDR=${_VERSION_HDR}
            -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
            -P ${_VERSION_SCRIPT}
    DEPENDS ${_VERSION_SCRIPT} ${CMAKE_SOURCE_DIR}/VERSION
    COMMENT "Resolving LBA2 version"
    VERBATIM
)

# An always-built target so the custom command above re-evaluates each build.
add_custom_target(lba2_version_gen ALL
    DEPENDS ${_VERSION_TXT} ${_VERSION_HDR})

# Configure-time seed: if the header doesn't exist yet (first configure),
# generate it now so the initial build doesn't fail on a missing #include
# from BUILD_INFO.h.
if(NOT EXISTS ${_VERSION_HDR})
    execute_process(
        COMMAND ${CMAKE_COMMAND}
                -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                -DOUT_TXT=${_VERSION_TXT}
                -DOUT_HDR=${_VERSION_HDR}
                -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                -P ${_VERSION_SCRIPT}
    )
endif()
