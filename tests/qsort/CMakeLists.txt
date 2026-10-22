# Host test for RetailQsort (LIB386/SYSTEM/QSORT.CPP), the 1997 game's qsort.
# One translation unit, no engine and no data, so it runs in CI on every
# platform, which is the point: the order it pins must be the same everywhere.
add_executable(test_retail_qsort test_retail_qsort.cpp)

target_sources(test_retail_qsort PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/QSORT.CPP)

target_include_directories(test_retail_qsort PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H    # SYSTEM/QSORT.H
    ${CMAKE_SOURCE_DIR}/tests       # test_harness.h
)
set_target_properties(test_retail_qsort PROPERTIES CXX_STANDARD 11)

add_test(NAME test_retail_qsort COMMAND test_retail_qsort)
register_host_test(test_retail_qsort)
