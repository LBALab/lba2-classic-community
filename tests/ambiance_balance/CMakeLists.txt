add_executable(test_ambiance_balance
    test_ambiance_balance.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/AUDIO_BALANCE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/REGLE3.CPP
)

target_include_directories(test_ambiance_balance PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
)

set_target_properties(test_ambiance_balance PROPERTIES CXX_STANDARD 11)

add_test(NAME test_ambiance_balance COMMAND test_ambiance_balance)

register_host_test(test_ambiance_balance)
