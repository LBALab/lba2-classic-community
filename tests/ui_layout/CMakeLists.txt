# Host test for the authored UI canvas' placement rule (SOURCES/UI_LAYOUT.H). Header-only: no
# engine sources to link, runs anywhere (Linux/macOS/Windows CI, no retail data). The UI's
# behavioural fixtures are the golden captures in tests/automation and need retail data, so this
# is the part of UI placement CI can see.
add_executable(test_ui_layout test_ui_layout.cpp)

target_include_directories(test_ui_layout PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES     # UI_LAYOUT.H
    ${CMAKE_SOURCE_DIR}/LIB386/H    # SYSTEM/ADELINE_TYPES.H (S32)
    ${CMAKE_SOURCE_DIR}/tests       # test_harness.h
)
target_compile_definitions(test_ui_layout PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)
set_target_properties(test_ui_layout PROPERTIES CXX_STANDARD 11)

add_test(NAME test_ui_layout COMMAND test_ui_layout)
register_host_test(test_ui_layout)
