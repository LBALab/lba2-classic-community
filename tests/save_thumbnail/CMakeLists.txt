add_executable(test_save_thumbnail
    test_save_thumbnail.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SVGA/SCALEBOX.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SVGA/SAVBLOCK.CPP
)

target_include_directories(test_save_thumbnail PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_save_thumbnail PROPERTIES CXX_STANDARD 11)

add_test(NAME test_save_thumbnail COMMAND test_save_thumbnail)

register_host_test(test_save_thumbnail)
