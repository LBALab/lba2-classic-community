cmake_minimum_required(VERSION 3.23)

enable_testing()

include(cmake/asm_test_helpers.cmake)

add_asm_cpp_test(
	NAME test_fire
	TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_fire.cpp
	ASM_SOURCE ${CMAKE_SOURCE_DIR}/SOURCES/FIRE.ASM
	RENAME_SYMS Do_Fire
	INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/SOURCES
)
target_sources(test_fire PRIVATE ${CMAKE_SOURCE_DIR}/SOURCES/FIRE.CPP)
target_sources(test_fire PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/UTILS.CPP)

add_asm_cpp_test(
	NAME test_compress
	TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_compress.cpp
	ASM_SOURCE ${CMAKE_SOURCE_DIR}/SOURCES/COMPRESS.ASM
	RENAME_SYMS AddString DeleteString
	INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/SOURCES
)
target_sources(test_compress PRIVATE ${CMAKE_SOURCE_DIR}/SOURCES/LZSS.CPP)

add_subdirectory(3D)
add_subdirectory(3DEXT)
add_subdirectory(SYSTEM)
add_subdirectory(MENU)
add_subdirectory(ANIM)
add_subdirectory(SVGA)
add_subdirectory(pol_work)
add_subdirectory(OBJECT)
add_subdirectory(SNAPSHOT)
add_subdirectory(fpu_precision)
