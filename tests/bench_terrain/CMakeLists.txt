# CPU micro-benchmark for the exterior terrain pass (manual target, not a
# ctest — it measures instead of asserting).  Drives the real engine math
# over a synthetic cube; run before/after a candidate optimization and
# compare the deltas.
add_executable(bench_terrain
    bench_terrain.cpp
)
target_include_directories(bench_terrain PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(bench_terrain PRIVATE 3D sys SDL3::SDL3)
set_target_properties(bench_terrain PROPERTIES CXX_STANDARD 11)
