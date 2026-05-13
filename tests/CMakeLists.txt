cmake_minimum_required(VERSION 3.23)

enable_testing()

# Aggregate target + label for host-only tests (no Docker, no retail data;
# run on Linux/macOS/Windows in CI). Each test registers itself via
# register_host_test(<target>) — adding a new host test is then two lines in
# its own CMakeLists.txt, with no workflow edits required.
#
#   cmake --build build --target host_tests   # builds every registered test
#   ctest -L host_quick --output-on-failure   # runs every registered test
add_custom_target(host_tests)

function(register_host_test test_name)
    add_dependencies(host_tests ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS host_quick)
endfunction()

add_subdirectory(discovery)
add_subdirectory(console)
add_subdirectory(credits)
add_subdirectory(copymask_negx)
add_subdirectory(version)
add_subdirectory(savegame)
add_subdirectory(deffile)

if(LBA2_BUILD_ASM_EQUIV_TESTS)
include(cmake/asm_test_helpers.cmake)

add_asm_cpp_test(
	NAME test_grille
	TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_grille.cpp
	ASM_SOURCE ${CMAKE_SOURCE_DIR}/SOURCES/GRILLE_A.ASM
	RENAME_SYMS DecompColonne Map2Screen GetAdrBlock WorldColBrickFull WorldCodeBrick GetBlockBrick GetWorldColBrickVisible WorldColBrick CodeJeu XMap YMap ZMap XScreen YScreen BufCube BufMap TabBlock BufferBrick
	INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/SOURCES
	LIBS 3dext 3D ail ani fio obj poly svg sys SDL3::SDL3
)
target_sources(test_grille PRIVATE ${CMAKE_SOURCE_DIR}/SOURCES/GRILLE.CPP)

add_asm_cpp_test(
	NAME test_plasma
	TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_plasma.cpp
	ASM_SOURCE ${CMAKE_SOURCE_DIR}/SOURCES/PLASMA.ASM
	RENAME_SYMS Do_Plasma Nb_Pts_Inter Nb_Pts_Control
	INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/SOURCES
)
target_sources(test_plasma PRIVATE ${CMAKE_SOURCE_DIR}/SOURCES/PLASMA.CPP)

add_asm_cpp_test(
	NAME test_func
	TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_func.cpp
	ASM_SOURCE ${CMAKE_SOURCE_DIR}/SOURCES/FUNC.ASM
	RENAME_SYMS SearchBoundColRGB
	INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/SOURCES
)
target_sources(test_func PRIVATE ${CMAKE_SOURCE_DIR}/SOURCES/FUNC.CPP)

add_asm_cpp_test(
	NAME test_flow_a
	TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_flow_a.cpp
	ASM_SOURCE ${CMAKE_SOURCE_DIR}/SOURCES/FLOW_A.ASM
	RENAME_SYMS BoxFlow ShadeBoxBlk CopyBlockShade
	INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/SOURCES
)
target_sources(test_flow_a PRIVATE ${CMAKE_SOURCE_DIR}/SOURCES/FLOW_A.CPP)

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
add_subdirectory(VIDEO_AUDIO)

endif()
