# Host test for the zone box predicate (SOURCES/ZONE_GEOM.H). Header-only: no
# engine sources to link, runs anywhere (Linux/macOS/Windows CI, no retail data).
add_executable(test_zone_geom test_zone_geom.cpp)

target_include_directories(test_zone_geom PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES     # ZONE_GEOM.H
    ${CMAKE_SOURCE_DIR}/LIB386/H    # SYSTEM/ADELINE_TYPES.H (S32)
    ${CMAKE_SOURCE_DIR}/tests       # test_harness.h
)
target_compile_definitions(test_zone_geom PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)
set_target_properties(test_zone_geom PROPERTIES CXX_STANDARD 11)

add_test(NAME test_zone_geom COMMAND test_zone_geom)
register_host_test(test_zone_geom)
