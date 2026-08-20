# AddDirIfNot sits in DIRECTORIES.CPP so this target can link it. EMBEDDED_CFG_WRITE
# and the generated cfg blob come with it, the same pair tests/discovery pulls in.
set(_USERDIRS_SOURCES
    ${CMAKE_SOURCE_DIR}/SOURCES/DIRECTORIES.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/EMBEDDED_CFG_WRITE.CPP
    ${CMAKE_BINARY_DIR}/SOURCES/embedded_lba2_cfg_data.cpp
)

add_executable(test_user_dirs test_user_dirs.cpp ${_USERDIRS_SOURCES})

target_include_directories(test_user_dirs PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(test_user_dirs PRIVATE sys SDL3::SDL3)

target_compile_definitions(test_user_dirs PRIVATE LBA2_BUILD_TESTS)

set_target_properties(test_user_dirs PROPERTIES CXX_STANDARD 11)

add_test(NAME test_user_dirs COMMAND test_user_dirs)

register_host_test(test_user_dirs)
