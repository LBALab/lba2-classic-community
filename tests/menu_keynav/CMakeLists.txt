add_executable(test_menu_keynav
    test_menu_keynav.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/MENU_KEYNAV.CPP)

target_include_directories(test_menu_keynav PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
)

add_test(NAME test_menu_keynav COMMAND test_menu_keynav)

register_host_test(test_menu_keynav)
