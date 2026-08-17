# Host test for the settings coercion rules (SOURCES/SETTINGS.H). Header-only: the rest of the
# settings layer talks to the cfg reader and the console, so only the coercion is reachable here,
# and it is the part where collapsing the rules changed what six settings boot into.
add_executable(test_settings_coerce test_settings_coerce.cpp)

target_include_directories(test_settings_coerce PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES  # SETTINGS.H
    ${CMAKE_SOURCE_DIR}/LIB386/H # SYSTEM/ADELINE_TYPES.H (S32)
    ${CMAKE_SOURCE_DIR}/tests    # test_harness.h
)
set_target_properties(test_settings_coerce PROPERTIES CXX_STANDARD 11)

add_test(NAME test_settings_coerce COMMAND test_settings_coerce)
register_host_test(test_settings_coerce)
