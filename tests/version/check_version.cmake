# Drift guard: assert build/VERSION.txt and build/VERSION_GENERATED.h are
# both non-empty AND carry the same payload byte-for-byte.
#
# Inputs (passed via -D from the test harness):
#   VERSION_FILE      — path to ${CMAKE_BINARY_DIR}/VERSION.txt
#   HEADER_FILE       — path to ${CMAKE_BINARY_DIR}/VERSION_GENERATED.h
#
# Failure modes this catches:
#   * The CMake script writes one but not the other.
#   * The two values diverge after a script edit (the whole point of having
#     both is they share a source of truth — this test enforces that).
#   * Either output is empty or absent.

if(NOT EXISTS "${VERSION_FILE}")
    message(FATAL_ERROR "VERSION_FILE not found: ${VERSION_FILE}")
endif()
if(NOT EXISTS "${HEADER_FILE}")
    message(FATAL_ERROR "HEADER_FILE not found: ${HEADER_FILE}")
endif()

file(READ "${VERSION_FILE}" _txt_raw)
string(STRIP "${_txt_raw}" _txt)
if(_txt STREQUAL "")
    message(FATAL_ERROR "VERSION_FILE is empty")
endif()

file(READ "${HEADER_FILE}" _hdr)
# Extract the value inside `#define LBA2_VERSION_STRING "..."`.
string(REGEX MATCH "#define[ \t]+LBA2_VERSION_STRING[ \t]+\"([^\"]*)\"" _match "${_hdr}")
if(NOT _match)
    message(FATAL_ERROR "HEADER_FILE does not define LBA2_VERSION_STRING in the expected shape")
endif()
set(_hdr_value "${CMAKE_MATCH_1}")
if(_hdr_value STREQUAL "")
    message(FATAL_ERROR "LBA2_VERSION_STRING is empty in header")
endif()

if(NOT "${_hdr_value}" STREQUAL "${_txt}")
    message(FATAL_ERROR
        "Drift detected between VERSION text and header macro:\n"
        "  text:   '${_txt}'\n"
        "  macro:  '${_hdr_value}'\n"
        "Both are written by cmake/write_version.cmake from the same source — investigate.")
endif()

message(STATUS "version drift check OK: '${_txt}'")
