cmake_minimum_required(VERSION 3.23)

enable_testing()

include(cmake/asm_test_helpers.cmake)

add_subdirectory(3D)
add_subdirectory(SYSTEM)
add_subdirectory(MENU)
add_subdirectory(ANIM)
add_subdirectory(SVGA)
add_subdirectory(pol_work)
add_subdirectory(OBJECT)
add_subdirectory(SNAPSHOT)
add_subdirectory(fpu_precision)
