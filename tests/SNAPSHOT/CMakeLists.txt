# ─────────────────────────────────────────────────────────────────────────────
# tests/SNAPSHOT/CMakeLists.txt
#
# Builds two standalone replay programs:
#   - replay_snapshot_asm  (links ALL LIB386 ASM files → libasm_lib386.a)
#   - replay_snapshot_cpp  (links existing CPP lib386 libraries)
#
# Then registers CTest tests that run compare_snapshots.sh for each
# .lba2snap fixture file in tests/SNAPSHOT/fixtures/.
# ─────────────────────────────────────────────────────────────────────────────

message(STATUS "SNAPSHOT: Configuring snapshot replay tests")
message(STATUS "SNAPSHOT: CMAKE_ASM_MASM_COMPILER = ${CMAKE_ASM_MASM_COMPILER}")
message(STATUS "SNAPSHOT: CMAKE_ASM_MASM_FLAGS = ${CMAKE_ASM_MASM_FLAGS}")

# ── Collect ALL ASM files from LIB386/ ──────────────────────────────────────
set(LIB386_DIR "${CMAKE_SOURCE_DIR}/LIB386")

set(LIB386_ASM_FILES
    # 3D/
    ${LIB386_DIR}/3D/COPYMATF.ASM
    ${LIB386_DIR}/3D/GETANG2D.ASM
    ${LIB386_DIR}/3D/GETANG3D.ASM
    ${LIB386_DIR}/3D/IMATTRAF.ASM
    ${LIB386_DIR}/3D/IMATSTDF.ASM
    ${LIB386_DIR}/3D/LITLISTF.ASM
    ${LIB386_DIR}/3D/LIROT3DF.ASM
    ${LIB386_DIR}/3D/LPROJISO.ASM
    ${LIB386_DIR}/3D/LPROJ3DF.ASM
    ${LIB386_DIR}/3D/LROT2DF.ASM
    ${LIB386_DIR}/3D/LROT3DF.ASM
    ${LIB386_DIR}/3D/MULMATF.ASM
    ${LIB386_DIR}/3D/PRLI3DF.ASM
    ${LIB386_DIR}/3D/PRLIISO.ASM
    ${LIB386_DIR}/3D/PROJ.ASM
    ${LIB386_DIR}/3D/REGLE3.ASM
    ${LIB386_DIR}/3D/ROTMATU.ASM
    ${LIB386_DIR}/3D/ROTVECT.ASM
    ${LIB386_DIR}/3D/ROTRALIF.ASM
    ${LIB386_DIR}/3D/SINTAB.ASM
    ${LIB386_DIR}/3D/SINTABF.ASM
    ${LIB386_DIR}/3D/SQRROOT.ASM
    ${LIB386_DIR}/3D/TANTAB.ASM
    # ANIM/
    ${LIB386_DIR}/ANIM/ANIM.ASM
    ${LIB386_DIR}/ANIM/BODY.ASM
    ${LIB386_DIR}/ANIM/CLEAR.ASM
    ${LIB386_DIR}/ANIM/FRAME.ASM
    ${LIB386_DIR}/ANIM/INTANIM.ASM
    ${LIB386_DIR}/ANIM/INTFRAME.ASM
    ${LIB386_DIR}/ANIM/INTERDEP.ASM
    ${LIB386_DIR}/ANIM/LIBINIT.ASM
    ${LIB386_DIR}/ANIM/STOFRAME.ASM
    ${LIB386_DIR}/ANIM/TEXTURE.ASM
    # MENU/
    ${LIB386_DIR}/MENU/SORT.ASM
    # OBJECT/
    ${LIB386_DIR}/OBJECT/AFF_OBJ.ASM
    # pol_work/
    ${LIB386_DIR}/pol_work/POLY.ASM
    ${LIB386_DIR}/pol_work/POLYCLIP.ASM
    ${LIB386_DIR}/pol_work/POLYDISC.ASM
    ${LIB386_DIR}/pol_work/POLYFLAT.ASM
    ${LIB386_DIR}/pol_work/POLYGTEX.ASM
    ${LIB386_DIR}/pol_work/POLYGOUR.ASM
    ${LIB386_DIR}/pol_work/POLYLINE.ASM
    ${LIB386_DIR}/pol_work/POLYTEXT.ASM
    ${LIB386_DIR}/pol_work/POLYTEXZ.ASM
    ${LIB386_DIR}/pol_work/POLYTZF.ASM
    ${LIB386_DIR}/pol_work/POLYTZG.ASM
    ${LIB386_DIR}/pol_work/POLY_JMP.ASM
    ${LIB386_DIR}/pol_work/TESTVUEF.ASM
    # SVGA/
    ${LIB386_DIR}/SVGA/AFFSTR.ASM
    ${LIB386_DIR}/SVGA/BLITBOXF.ASM
    ${LIB386_DIR}/SVGA/BOX.ASM
    ${LIB386_DIR}/SVGA/CALCMASK.ASM
    ${LIB386_DIR}/SVGA/CLRBOXF.ASM
    ${LIB386_DIR}/SVGA/COPYMASK.ASM
    ${LIB386_DIR}/SVGA/CPYBLOCK.ASM
    ${LIB386_DIR}/SVGA/CPYBLOCI.ASM
    ${LIB386_DIR}/SVGA/FONT.ASM
    ${LIB386_DIR}/SVGA/GRAPH.ASM
    ${LIB386_DIR}/SVGA/MASK.ASM
    ${LIB386_DIR}/SVGA/RESBLOCK.ASM
    ${LIB386_DIR}/SVGA/SAVBLOCK.ASM
    ${LIB386_DIR}/SVGA/SCALEBOX.ASM
    ${LIB386_DIR}/SVGA/SCALESPI.ASM
    ${LIB386_DIR}/SVGA/SCALESPT.ASM
    # SYSTEM/
    ${LIB386_DIR}/SYSTEM/CPU.ASM
    ${LIB386_DIR}/SYSTEM/FASTCPYF.ASM
    ${LIB386_DIR}/SYSTEM/LZ.ASM
    ${LIB386_DIR}/SYSTEM/MOUSEDAT.ASM
)

# ── Assemble each ASM file into an object using the same approach as
#    asm_test_helpers.cmake: patch SMALL→FLAT, then assemble with UASM ────

set(ASM_LIB_OBJECTS "")

# Split CMAKE_ASM_MASM_FLAGS into a proper list
separate_arguments(_uasm_flags NATIVE_COMMAND "${CMAKE_ASM_MASM_FLAGS}")

foreach(_asm_file ${LIB386_ASM_FILES})
    get_filename_component(_asm_name ${_asm_file} NAME_WE)
    get_filename_component(_asm_dir ${_asm_file} DIRECTORY)

    # Make unique output names by including subdirectory
    file(RELATIVE_PATH _asm_rel ${LIB386_DIR} ${_asm_file})
    string(REPLACE "/" "_" _asm_prefix "${_asm_rel}")
    string(REPLACE ".ASM" "" _asm_prefix "${_asm_prefix}")

    set(_patched "${CMAKE_CURRENT_BINARY_DIR}/${_asm_prefix}_flat.ASM")
    set(_obj     "${CMAKE_CURRENT_BINARY_DIR}/${_asm_prefix}.asm.o")

    # Patch .model SMALL → .model FLAT
    add_custom_command(
        OUTPUT ${_patched}
        COMMAND ${CMAKE_COMMAND}
                -DSRC=${_asm_file}
                -DDST=${_patched}
                -P ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_flat.cmake
        DEPENDS ${_asm_file}
                ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_flat.cmake
        COMMENT "Patching ${_asm_prefix}: .model SMALL → FLAT"
        VERBATIM
    )

    # Assemble with UASM
    add_custom_command(
        OUTPUT ${_obj}
        COMMAND ${CMAKE_ASM_MASM_COMPILER}
                ${_uasm_flags}
                -I${CMAKE_SOURCE_DIR}/LIB386/H
                -I${_asm_dir}
                -Fo${_obj}
                ${_patched}
        DEPENDS ${_patched}
        COMMENT "Assembling ${_asm_prefix}"
        VERBATIM
    )

    list(APPEND ASM_LIB_OBJECTS ${_obj})
endforeach()

# ── Build libasm_lib386.a static library from all ASM objects ───────────────

# Create a custom target that depends on all ASM objects
add_custom_target(asm_lib386_objects DEPENDS ${ASM_LIB_OBJECTS})

# Manually create the static library using ar
set(ASM_LIB386 "${CMAKE_CURRENT_BINARY_DIR}/libasm_lib386.a")
add_custom_command(
    OUTPUT ${ASM_LIB386}
    COMMAND ${CMAKE_AR} rcs ${ASM_LIB386} ${ASM_LIB_OBJECTS}
    DEPENDS ${ASM_LIB_OBJECTS}
    COMMENT "Creating libasm_lib386.a from all ASM objects"
    VERBATIM
)
add_custom_target(asm_lib386 ALL DEPENDS ${ASM_LIB386})

# ── Shared replay source files ──────────────────────────────────────────────
set(REPLAY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/snapshot_replay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/replay_snapshot_main.cpp
)

set(REPLAY_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)

# ── replay_snapshot_cpp — links CPP implementations ─────────────────────────
add_executable(replay_snapshot_cpp ${REPLAY_SOURCES}
    ${CMAKE_SOURCE_DIR}/LIB386/SNAPSHOT/SNAPSHOT.CPP
)
target_include_directories(replay_snapshot_cpp PRIVATE ${REPLAY_INCLUDES})
target_link_libraries(replay_snapshot_cpp PRIVATE
    3D obj poly svg ani sys m
)
target_compile_definitions(replay_snapshot_cpp PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)
set_target_properties(replay_snapshot_cpp PROPERTIES
    POSITION_INDEPENDENT_CODE OFF
)
target_link_options(replay_snapshot_cpp PRIVATE -no-pie)

# ── replay_snapshot_asm — links ASM implementations ─────────────────────────
# The ASM library provides function implementations (rendering, math, etc.).
# We link the CPP libraries too for globals and initialization functions
# (Switch_Fillers, SetScreenPitch, etc.). --allow-multiple-definition lets
# the ASM implementations override the CPP ones for functions defined in both.
add_executable(replay_snapshot_asm ${REPLAY_SOURCES}
    ${CMAKE_SOURCE_DIR}/LIB386/SNAPSHOT/SNAPSHOT.CPP
)
target_include_directories(replay_snapshot_asm PRIVATE ${REPLAY_INCLUDES})
target_link_libraries(replay_snapshot_asm PRIVATE
    ${ASM_LIB386} 3D obj poly svg ani sys m
)
add_dependencies(replay_snapshot_asm asm_lib386)
target_compile_definitions(replay_snapshot_asm PRIVATE
    "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
)
set_target_properties(replay_snapshot_asm PROPERTIES
    POSITION_INDEPENDENT_CODE OFF
)
target_link_options(replay_snapshot_asm PRIVATE -no-pie -Wl,--allow-multiple-definition)

# ── Register comparison tests for each fixture ──────────────────────────────
set(COMPARE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/compare_snapshots.sh")

file(GLOB SNAPSHOT_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/*.lba2snap")
message(STATUS "SNAPSHOT: Found ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/*.lba2snap -> ${SNAPSHOT_FIXTURES}")
foreach(_fixture ${SNAPSHOT_FIXTURES})
    get_filename_component(_fixture_name ${_fixture} NAME_WE)
    add_test(
        NAME test_snapshot_${_fixture_name}
        COMMAND bash ${COMPARE_SCRIPT}
                ${_fixture}
                $<TARGET_FILE:replay_snapshot_asm>
                $<TARGET_FILE:replay_snapshot_cpp>
    )
endforeach()
