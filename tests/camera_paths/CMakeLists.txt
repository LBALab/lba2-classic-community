# The float and integer cameras CAMERA.CPP keeps side by side, and the two
# camera paths over them (LIB386/3D/CAMERA.CPP). Host only: the integer ports
# are ASM-checked in tests/3D.
add_executable(test_camera_paths test_camera_paths.cpp)
target_include_directories(test_camera_paths PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
target_link_libraries(test_camera_paths PRIVATE 3D sys)
set_target_properties(test_camera_paths PROPERTIES CXX_STANDARD 11)
add_test(NAME test_camera_paths COMMAND test_camera_paths)
register_host_test(test_camera_paths)
