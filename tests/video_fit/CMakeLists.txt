add_executable(test_video_fit test_video_fit.cpp)

target_include_directories(test_video_fit PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
)

set_target_properties(test_video_fit PROPERTIES CXX_STANDARD 11)

add_test(NAME test_video_fit COMMAND test_video_fit)

register_host_test(test_video_fit)
