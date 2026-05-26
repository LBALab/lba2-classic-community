add_executable(test_demo_rng_seed test_demo_rng_seed.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/DEMO_SEED.CPP)

target_include_directories(test_demo_rng_seed PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_demo_rng_seed PROPERTIES CXX_STANDARD 11)

add_test(NAME test_demo_rng_seed COMMAND test_demo_rng_seed)

register_host_test(test_demo_rng_seed)
