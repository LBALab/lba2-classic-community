# Host regression test for body-primitive geometry (issue #357).
#
# Compiles the real LIB386/OBJECT/AFF_OBJ.CPP and links the real 3D math library,
# then drives Sphere()/Sphere_Transp() directly and asserts the radius they emit
# against the original AFF_OBJ.ASM formula. The test file provides spy/stub
# definitions for the pol_work fillers (Fill_Sphere spy, Fill_Poly/Line_A stubs)
# so no framebuffer, palette, or game assets are needed - a self-contained host
# build, no SDL, no Docker.
add_executable(test_sphere_radius
    test_sphere_radius.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/OBJECT/AFF_OBJ.CPP
)

target_include_directories(test_sphere_radius PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_BINARY_DIR}
)

# AFF_OBJ.CPP's rotation/projection/camera externs come from the 3D library.
target_link_libraries(test_sphere_radius PRIVATE 3D)

set_target_properties(test_sphere_radius PROPERTIES CXX_STANDARD 11)

add_test(NAME test_sphere_radius COMMAND test_sphere_radius)
register_host_test(test_sphere_radius)
