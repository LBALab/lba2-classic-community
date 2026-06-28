# RES_PICKER.CPP defines the message bodies and the Android gate; DIRECTORIES.CPP
# supplies IsValidResourceDir that RES_PICKER references. Android_GetExternalFilesDir
# and Log_* come from the `sys` library; SDL3 satisfies the picker's SDL calls at
# link time (the test never calls PromptForResDir, so no display is needed).
set(_PICKER_SOURCES
    ${CMAKE_SOURCE_DIR}/SOURCES/RES_PICKER.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/DIRECTORIES.CPP
)

add_executable(test_res_picker test_res_picker.cpp ${_PICKER_SOURCES})

target_include_directories(test_res_picker PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(test_res_picker PRIVATE sys SDL3::SDL3)

target_compile_definitions(test_res_picker PRIVATE
    LBA2_BUILD_TESTS
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)

set_target_properties(test_res_picker PROPERTIES CXX_STANDARD 11)

add_test(NAME test_res_picker COMMAND test_res_picker)

register_host_test(test_res_picker)
