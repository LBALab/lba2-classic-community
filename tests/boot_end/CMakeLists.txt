# Host test for the exit report (SOURCES/BOOT_END.H). Header-only: no engine sources to link,
# runs anywhere (Linux/macOS/Windows CI, no retail data). The failure paths it covers can
# otherwise only be reached by breaking an install, which is why they were unguarded.
add_executable(test_boot_end test_boot_end.cpp)

target_include_directories(test_boot_end PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES  # BOOT_END.H
    ${CMAKE_SOURCE_DIR}/LIB386/H # SYSTEM/ADELINE_TYPES.H (S32)
    ${CMAKE_SOURCE_DIR}/tests    # test_harness.h
)
target_compile_definitions(test_boot_end PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)
set_target_properties(test_boot_end PROPERTIES CXX_STANDARD 11)

add_test(NAME test_boot_end COMMAND test_boot_end)
register_host_test(test_boot_end)
