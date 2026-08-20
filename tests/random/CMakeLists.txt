# Host test for the engine's pseudo-random stream (LIB386/SYSTEM/RANDOM.CPP).
# Links that one translation unit and nothing else: pure integer arithmetic, no
# engine, no SDL, no retail data, so it runs in CI on every platform. Running
# everywhere is the point, since what it pins is that every platform draws the
# same numbers.
add_executable(test_rnd_stream test_rnd_stream.cpp)

target_sources(test_rnd_stream PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/RANDOM.CPP)

target_include_directories(test_rnd_stream PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H    # SYSTEM/RANDOM.H, SYSTEM/ADELINE_TYPES.H
    ${CMAKE_SOURCE_DIR}/tests       # test_harness.h
)
set_target_properties(test_rnd_stream PROPERTIES CXX_STANDARD 11)

add_test(NAME test_rnd_stream COMMAND test_rnd_stream)
register_host_test(test_rnd_stream)
