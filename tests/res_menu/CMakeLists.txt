add_executable(test_res_menu
    test_res_menu.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/RES_MENU.CPP
)

target_include_directories(test_res_menu PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_res_menu PROPERTIES CXX_STANDARD 11)

add_test(NAME test_res_menu COMMAND test_res_menu)

register_host_test(test_res_menu)
