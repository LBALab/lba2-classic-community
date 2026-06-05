add_executable(test_focus_audio test_focus_audio.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FOCUS_AUDIO.CPP)

target_include_directories(test_focus_audio PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_focus_audio PROPERTIES CXX_STANDARD 11)

add_test(NAME test_focus_audio COMMAND test_focus_audio)

register_host_test(test_focus_audio)
