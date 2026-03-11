# ─────────────────────────────────────────────────────────────────────────────
# asm_test_helpers.cmake
#
# Provides add_asm_cpp_test() — builds a test that links both the ASM and CPP
# implementations of a LIB386 function, using objcopy to rename the ASM symbols
# so both can coexist in the same binary.
#
# ALL tests are ASM-vs-CPP equivalence tests.  There is no CPP-only mode.
# Tests must be run inside Docker via run_tests_docker.sh.
# ─────────────────────────────────────────────────────────────────────────────

# find objcopy once
find_program(OBJCOPY_EXECUTABLE objcopy)
if(NOT OBJCOPY_EXECUTABLE)
    message(FATAL_ERROR "objcopy not found — needed for ASM equivalence tests")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# add_asm_cpp_test(
#     NAME            <test-target-name>
#     TEST_SOURCE     <test .cpp file>
#     ASM_SOURCE      <original .ASM file>
#     RENAME_SYMS     <sym1> [sym2 ...]      # symbols to rename  Foo -> asm_Foo
#     LIBS            <lib1> [lib2 ...]       # CPP libraries to link
#     [INCLUDE_DIRS   <dir1> ...]             # extra include dirs
#     [ASM_INCLUDE_DIRS <dir1> ...]           # include dirs for UASM
#     [ASM_DEPS       <file1.ASM> ...]        # extra ASM data dependencies
#     [ASM_DEP_RENAME_SYMS <sym1> ...]        # symbols to rename in BOTH main + dep objects
#     [STRIP_C_ADAPT]                         # strip C-adapted PROC wrappers → bare Watcom procs
# )
# ─────────────────────────────────────────────────────────────────────────────
function(add_asm_cpp_test)
    cmake_parse_arguments(
        ARG                          # prefix
        "STRIP_C_ADAPT"              # options (booleans)
        "NAME;TEST_SOURCE;ASM_SOURCE" # one-value keywords
        "RENAME_SYMS;LIBS;INCLUDE_DIRS;ASM_INCLUDE_DIRS;ASM_DEPS;ASM_DEP_RENAME_SYMS" # multi-value keywords
        ${ARGN}
    )

    # ── CPP-only test (always built) ──────────────────────────────────────
    add_executable(${ARG_NAME} ${ARG_TEST_SOURCE})
    target_include_directories(${ARG_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/LIB386/H
        ${CMAKE_SOURCE_DIR}/tests
        ${ARG_INCLUDE_DIRS}
    )
    target_link_libraries(${ARG_NAME} PRIVATE ${ARG_LIBS})
    target_link_libraries(${ARG_NAME} PRIVATE m)  # math lib
    # Pass source root so test_harness.h can strip it from __FILE__ paths.
    target_compile_definitions(${ARG_NAME} PRIVATE
        "TEST_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}/\""
    )
    add_test(NAME ${ARG_NAME} COMMAND ${ARG_NAME})

    # ── ASM equivalence ──────────────────────────────────────────────────
    if(ARG_ASM_SOURCE)

        # ASM objects may contain text relocations (R_386_32 in .text) that
        # are incompatible with PIE.  Disable PIE for ASM test binaries.
        set_target_properties(${ARG_NAME} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
        target_link_options(${ARG_NAME} PRIVATE -no-pie)

        # Assemble the ASM file ------------------------------------------
        get_filename_component(_asm_name ${ARG_ASM_SOURCE} NAME_WE)
        get_filename_component(_asm_dir  ${ARG_ASM_SOURCE} DIRECTORY)
        set(_asm_obj "${CMAKE_CURRENT_BINARY_DIR}/${_asm_name}.asm.o")
        set(_asm_obj_renamed "${CMAKE_CURRENT_BINARY_DIR}/${_asm_name}.asm.renamed.o")

        # Patch .model SMALL → .model FLAT so the ASM produces a valid
        # 32-bit ELF object (identical instruction encoding, different
        # relocation metadata).  Files already using FLAT are unchanged.
        # If STRIP_C_ADAPT is set, first strip C-adapted PROC wrappers
        # back to bare Watcom register-convention procs.
        set(_asm_src_input ${ARG_ASM_SOURCE})
        if(ARG_STRIP_C_ADAPT)
            set(_asm_src_watcom "${CMAKE_CURRENT_BINARY_DIR}/${_asm_name}_watcom.ASM")
            add_custom_command(
                OUTPUT ${_asm_src_watcom}
                COMMAND ${CMAKE_COMMAND}
                        -DSRC=${ARG_ASM_SOURCE}
                        -DDST=${_asm_src_watcom}
                        -P ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_watcom.cmake
                DEPENDS ${ARG_ASM_SOURCE}
                        ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_watcom.cmake
                COMMENT "Stripping C-adaptation from ${_asm_name}"
                VERBATIM
            )
            set(_asm_src_input ${_asm_src_watcom})
        endif()

        set(_asm_src_patched "${CMAKE_CURRENT_BINARY_DIR}/${_asm_name}_flat.ASM")
        add_custom_command(
            OUTPUT ${_asm_src_patched}
            COMMAND ${CMAKE_COMMAND}
                    -DSRC=${_asm_src_input}
                    -DDST=${_asm_src_patched}
                    -P ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_flat.cmake
            DEPENDS ${_asm_src_input}
                    ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_flat.cmake
            COMMENT "Patching ${_asm_name}: .model SMALL → FLAT"
            VERBATIM
        )

        # Build UASM include flags
        set(_uasm_includes "")
        foreach(_dir ${ARG_ASM_INCLUDE_DIRS})
            list(APPEND _uasm_includes "-I${_dir}")
        endforeach()
        # Always include LIB386/H and the original ASM source directory
        list(APPEND _uasm_includes "-I${CMAKE_SOURCE_DIR}/LIB386/H")
        list(APPEND _uasm_includes "-I${_asm_dir}")

        # Split CMAKE_ASM_MASM_FLAGS (a space-separated string) into a proper list
        separate_arguments(_uasm_flags NATIVE_COMMAND "${CMAKE_ASM_MASM_FLAGS}")

        add_custom_command(
            OUTPUT ${_asm_obj}
            COMMAND ${CMAKE_ASM_MASM_COMPILER}
                    ${_uasm_flags}
                    ${_uasm_includes}
                    -Fo${_asm_obj}
                    ${_asm_src_patched}
            DEPENDS ${_asm_src_patched}
            COMMENT "Assembling ${_asm_name} (patched)"
            VERBATIM
        )

        # Build --redefine-sym flags ------------------------------------
        set(_rename_flags "")
        foreach(_sym ${ARG_RENAME_SYMS})
            list(APPEND _rename_flags "--redefine-sym" "${_sym}=asm_${_sym}")
        endforeach()
        # ASM_DEP_RENAME_SYMS are applied to BOTH the main object and dep
        # objects so that cross-references resolve consistently while the
        # CPP library symbols remain untouched.
        foreach(_sym ${ARG_ASM_DEP_RENAME_SYMS})
            list(APPEND _rename_flags "--redefine-sym" "${_sym}=asm_${_sym}")
        endforeach()

        add_custom_command(
            OUTPUT ${_asm_obj_renamed}
            COMMAND ${OBJCOPY_EXECUTABLE} ${_rename_flags}
                    ${_asm_obj} ${_asm_obj_renamed}
            DEPENDS ${_asm_obj}
            COMMENT "Renaming ASM symbols in ${_asm_name}"
            VERBATIM
        )

        # Link the renamed object into the test -------------------------
        target_sources(${ARG_NAME} PRIVATE ${_asm_obj_renamed})
        set_source_files_properties(${_asm_obj_renamed} PROPERTIES
            EXTERNAL_OBJECT TRUE
            GENERATED TRUE
        )

        # ── Extra ASM data dependencies (e.g. SinTabF data tables) ────
        # These are assembled and their exported symbols are renamed via
        # ASM_DEP_RENAME_SYMS so they don't collide with CPP library
        # definitions.  The main ASM object's references to those symbols
        # are also renamed (see above) so cross-references still resolve.
        # Build dep rename flags
        set(_dep_rename_flags "")
        foreach(_sym ${ARG_ASM_DEP_RENAME_SYMS})
            list(APPEND _dep_rename_flags "--redefine-sym" "${_sym}=asm_${_sym}")
        endforeach()

        foreach(_dep ${ARG_ASM_DEPS})
            get_filename_component(_dep_name ${_dep} NAME_WE)
            get_filename_component(_dep_dir  ${_dep} DIRECTORY)
            set(_dep_input   ${_dep})
            # Use test name as prefix to avoid output path collisions
            # when multiple tests share the same dep ASM file.
            set(_dep_prefix  "${ARG_NAME}_${_dep_name}")
            set(_dep_patched "${CMAKE_CURRENT_BINARY_DIR}/${_dep_prefix}_flat_dep.ASM")
            set(_dep_obj     "${CMAKE_CURRENT_BINARY_DIR}/${_dep_prefix}.dep.o")
            set(_dep_obj_renamed "${CMAKE_CURRENT_BINARY_DIR}/${_dep_prefix}.dep.renamed.o")

            # Optionally strip C-adapted PROC wrappers from deps too
            if(ARG_STRIP_C_ADAPT)
                set(_dep_watcom "${CMAKE_CURRENT_BINARY_DIR}/${_dep_prefix}_watcom_dep.ASM")
                add_custom_command(
                    OUTPUT ${_dep_watcom}
                    COMMAND ${CMAKE_COMMAND}
                            -DSRC=${_dep}
                            -DDST=${_dep_watcom}
                            -P ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_watcom.cmake
                    DEPENDS ${_dep}
                            ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_watcom.cmake
                    COMMENT "Stripping C-adaptation from dep ${_dep_name}"
                    VERBATIM
                )
                set(_dep_input ${_dep_watcom})
            endif()

            add_custom_command(
                OUTPUT ${_dep_patched}
                COMMAND ${CMAKE_COMMAND}
                        -DSRC=${_dep_input}
                        -DDST=${_dep_patched}
                        -P ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_flat.cmake
                DEPENDS ${_dep_input}
                        ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_flat.cmake
                COMMENT "Patching dep ${_dep_name}: .model SMALL → FLAT"
                VERBATIM
            )

            set(_dep_uasm_includes "-I${CMAKE_SOURCE_DIR}/LIB386/H" "-I${_dep_dir}")
            add_custom_command(
                OUTPUT ${_dep_obj}
                COMMAND ${CMAKE_ASM_MASM_COMPILER}
                        ${_uasm_flags}
                        ${_dep_uasm_includes}
                        -Fo${_dep_obj}
                        ${_dep_patched}
                DEPENDS ${_dep_patched}
                COMMENT "Assembling dep ${_dep_name} (patched)"
                VERBATIM
            )

            if(_dep_rename_flags)
                add_custom_command(
                    OUTPUT ${_dep_obj_renamed}
                    COMMAND ${OBJCOPY_EXECUTABLE} ${_dep_rename_flags}
                            ${_dep_obj} ${_dep_obj_renamed}
                    DEPENDS ${_dep_obj}
                    COMMENT "Renaming dep symbols in ${_dep_name}"
                    VERBATIM
                )
                set(_dep_final ${_dep_obj_renamed})
            else()
                set(_dep_final ${_dep_obj})
            endif()

            target_sources(${ARG_NAME} PRIVATE ${_dep_final})
            set_source_files_properties(${_dep_final} PROPERTIES
                EXTERNAL_OBJECT TRUE
                GENERATED TRUE
            )
        endforeach()

        if(ARG_ASM_DEPS AND NOT _dep_rename_flags)
            target_link_options(${ARG_NAME} PRIVATE -Wl,--allow-multiple-definition)
        endif()

    endif()
endfunction()
