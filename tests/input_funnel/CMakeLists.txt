add_executable(test_input_funnel
    test_input_funnel.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP)

target_include_directories(test_input_funnel PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
)

add_test(NAME test_input_funnel COMMAND test_input_funnel)

register_host_test(test_input_funnel)
