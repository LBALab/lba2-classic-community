# Host regression: the double-wide stereo gate (LIB386/H/GL/STEREO_POLICY.H).
#
# The isometric interior only gets parallax on the double-wide single-pass
# path, because its affine projection ignores the per-eye camera shift the
# per-eye fallback relies on.  The gate must therefore accept AA off, the
# shipping default, and leave only FXAA on the per-eye path.  Header-only, no
# GL context, no game assets: runs in host_quick CI on every platform.
add_executable(test_stereo_policy
    test_stereo_policy.cpp
)
target_include_directories(test_stereo_policy PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)
set_target_properties(test_stereo_policy PROPERTIES CXX_STANDARD 11)
add_test(NAME test_stereo_policy COMMAND test_stereo_policy)
register_host_test(test_stereo_policy)
