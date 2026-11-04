add_executable(test_xbr test_xbr.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SVGA/XBR.CPP)

target_include_directories(test_xbr PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_xbr PROPERTIES CXX_STANDARD 11)

add_test(NAME test_xbr COMMAND test_xbr)

register_host_test(test_xbr)
