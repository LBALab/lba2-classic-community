include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/vita-version.cmake")

if(DEFINED INVALID_VERSION)
    lba2_vita_package_version("${INVALID_VERSION}" result)
    message(FATAL_ERROR "Unexpectedly accepted '${INVALID_VERSION}'.")
endif()

function(expect_version input expected)
    lba2_vita_package_version("${input}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "'${input}': expected '${expected}', got '${actual}'.")
    endif()
endfunction()

# A stale cache from an older build must not override the derived version.
set(LBA2_VITA_APP_VERSION "00.01" CACHE STRING "")
lba2_vita_package_version("0.13.2-dev" LBA2_VITA_APP_VERSION)
if(NOT LBA2_VITA_APP_VERSION STREQUAL "00.13")
    message(FATAL_ERROR "Cached package version overrode the engine version.")
endif()

expect_version("0.13.2" "00.13")
expect_version("0.13.99" "00.13")
expect_version("0.13.2-dev-dirty" "00.13")
expect_version("0.13.2-rc.1+build.42" "00.13")
expect_version("1.0.0" "01.00")
expect_version("9.8.7" "09.08")
expect_version("10.20.30" "10.20")
expect_version("99.99.99" "99.99")

function(expect_rejected input diagnostic)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DINVALID_VERSION=${input}" -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(status EQUAL 0 OR NOT "${output}${error}" MATCHES "${diagnostic}")
        message(FATAL_ERROR "Expected rejection of '${input}': ${output}${error}")
    endif()
endfunction()

expect_rejected("" "Invalid engine version")
expect_rejected("invalid" "Invalid engine version")
expect_rejected("1.2" "Invalid engine version")
expect_rejected("-1.2.3" "Invalid engine version")
expect_rejected("100.0.0" "major and minor must each fit in two digits")
expect_rejected("0.100.0" "major and minor must each fit in two digits")
