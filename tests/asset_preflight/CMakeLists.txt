add_executable(test_asset_preflight
    test_asset_preflight.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/ASSET_PREFLIGHT.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/DIRECTORIES.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/LOG/LOG.CPP)

target_include_directories(test_asset_preflight PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests)

# Log core bypasses SDL (no SDL_Init needed); DIRECTORIES resolvers use sys + SDL.
target_compile_definitions(test_asset_preflight PRIVATE LBA_LOG_NO_SDL LBA2_BUILD_TESTS)

target_link_libraries(test_asset_preflight PRIVATE sys SDL3::SDL3)

set_target_properties(test_asset_preflight PROPERTIES CXX_STANDARD 11)

add_test(NAME test_asset_preflight COMMAND test_asset_preflight)
register_host_test(test_asset_preflight)
