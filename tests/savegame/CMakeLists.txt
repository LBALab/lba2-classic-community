add_executable(test_savegame_load_bounds test_load_bounds.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/SAVEGAME_LOAD_BOUNDS.CPP)

target_include_directories(test_savegame_load_bounds PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_savegame_load_bounds PROPERTIES CXX_STANDARD 11)

add_test(NAME test_savegame_load_bounds COMMAND test_savegame_load_bounds)

register_host_test(test_savegame_load_bounds)

add_executable(test_save_name_validation test_save_name_validation.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/SAVENAME_VALIDATION.CPP)

target_include_directories(test_save_name_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_save_name_validation PROPERTIES CXX_STANDARD 11)

add_test(NAME test_save_name_validation COMMAND test_save_name_validation)

register_host_test(test_save_name_validation)
