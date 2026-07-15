# Host regression tests for body-primitive geometry (issue #357 class).
#
# Each target compiles the real LIB386/OBJECT/AFF_OBJ.CPP + the shared harness
# (spies for the pol_work fillers) and links the real 3D math library, then drives
# one primitive directly and asserts the geometry it emits against the original
# AFF_OBJ.ASM formula. No framebuffer, palette, or game assets: a self-contained
# host build, no SDL, no Docker. See primitive_harness.h for the design.
function(add_render_primitive_test test_name)
    add_executable(${test_name}
        ${test_name}.cpp
        primitive_harness.cpp
        ${CMAKE_SOURCE_DIR}/LIB386/OBJECT/AFF_OBJ.CPP
    )
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/LIB386/H
        ${CMAKE_BINARY_DIR}
    )
    # AFF_OBJ.CPP's rotation/projection/camera externs come from the 3D library.
    target_link_libraries(${test_name} PRIVATE 3D)
    set_target_properties(${test_name} PROPERTIES CXX_STANDARD 11)
    add_test(NAME ${test_name} COMMAND ${test_name})
    register_host_test(${test_name})
endfunction()

add_render_primitive_test(test_sphere_radius)
add_render_primitive_test(test_line_geometry)
add_render_primitive_test(test_poly_geometry)

# Render-interpolation core (issue #412) -- pure math, no ASM oracle, no SDL.
add_executable(test_render_interp test_render_interp.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/3D/RENDERINTERP.CPP)
target_include_directories(test_render_interp PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
set_target_properties(test_render_interp PROPERTIES CXX_STANDARD 11)
add_test(NAME test_render_interp COMMAND test_render_interp)
register_host_test(test_render_interp)
