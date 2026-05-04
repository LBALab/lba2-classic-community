set(_DISC_SOURCES
    ${CMAKE_SOURCE_DIR}/SOURCES/RES_DISCOVERY.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/DIRECTORIES.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/EMBEDDED_CFG_WRITE.CPP
    ${CMAKE_BINARY_DIR}/SOURCES/embedded_lba2_cfg_data.cpp
)

add_executable(test_res_discovery test_res_discovery.cpp ${_DISC_SOURCES})

target_include_directories(test_res_discovery PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(test_res_discovery PRIVATE sys SDL3::SDL3)

target_compile_definitions(test_res_discovery PRIVATE
    LBA2_BUILD_TESTS
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)

set_target_properties(test_res_discovery PROPERTIES CXX_STANDARD 11)

add_test(NAME test_res_discovery COMMAND test_res_discovery)

add_executable(test_savegame_load_bounds test_savegame_load_bounds.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/SAVEGAME_LOAD_BOUNDS.CPP)

target_include_directories(test_savegame_load_bounds PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_savegame_load_bounds PROPERTIES CXX_STANDARD 11)

add_test(NAME test_savegame_load_bounds COMMAND test_savegame_load_bounds)
