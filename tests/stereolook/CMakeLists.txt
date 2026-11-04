# Host test: stereo head-look camera math (SOURCES/STEREOLOOK.CPP).
#
# Pins the convention set the tracked-pose layering depends on: the tracked
# head translation must land on the axes the engine's own LongRotate uses
# (forward along 2048-BetaCam, right 90 degrees off it), and the view-angle
# composition must wrap like the rig's BetaCam.  CPP-only (no ASM, no
# Docker, no game assets), so it runs in host_quick CI on every platform.
add_executable(test_stereolook
    test_stereolook.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/STEREOLOOK.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/LROT2DF.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/SINTABF.CPP
)
target_include_directories(test_stereolook PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/tests
)
set_target_properties(test_stereolook PROPERTIES CXX_STANDARD 11)
add_test(NAME test_stereolook COMMAND test_stereolook)
register_host_test(test_stereolook)
