add_executable(test_menu_labels
    test_menu_labels.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/MENU_LABELS.CPP
)

target_include_directories(test_menu_labels PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
)

set_target_properties(test_menu_labels PROPERTIES CXX_STANDARD 11)

add_test(NAME test_menu_labels COMMAND test_menu_labels)

register_host_test(test_menu_labels)
