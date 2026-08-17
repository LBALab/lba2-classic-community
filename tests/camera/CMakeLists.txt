# Host test for the Auto camera's angle arithmetic (SOURCES/FOLLOWCAM_MATH.H). Header-only: no
# engine sources to link, runs anywhere (Linux/macOS/Windows CI, no retail data). The camera's
# behavioural fixtures live in tests/automation and need retail data, so this is the part of the
# camera CI can see.
add_executable(test_followcam_math test_followcam_math.cpp)

target_include_directories(test_followcam_math PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES     # FOLLOWCAM_MATH.H, FOLLOWCAM_CFG.H
    ${CMAKE_SOURCE_DIR}/LIB386/H    # SYSTEM/ADELINE_TYPES.H (S32)
    ${CMAKE_SOURCE_DIR}/tests       # test_harness.h
)
target_compile_definitions(test_followcam_math PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)
set_target_properties(test_followcam_math PROPERTIES CXX_STANDARD 11)

add_test(NAME test_followcam_math COMMAND test_followcam_math)
register_host_test(test_followcam_math)
