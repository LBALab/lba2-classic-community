# ─────────────────────────────────────────────────────────────────────────────
# asm_test_helpers.cmake
#
# Provides add_asm_cpp_test() — builds a test that links both the ASM and CPP
# implementations of a LIB386 function, using objcopy to rename the ASM symbols
# so both can coexist in the same binary.
# ─────────────────────────────────────────────────────────────────────────────

# find objcopy once
if(LBA2_BUILD_ASM_TESTS)
    find_program(OBJCOPY_EXECUTABLE objcopy)
    if(NOT OBJCOPY_EXECUTABLE)
        message(FATAL_ERROR "objcopy not found — needed for ASM equivalence tests")
    endif()
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
# )
#
# When LBA2_BUILD_ASM_TESTS is OFF the ASM parts are skipped and only the CPP
# correctness test is built.
# ─────────────────────────────────────────────────────────────────────────────
function(add_asm_cpp_test)
    cmake_parse_arguments(
        ARG                          # prefix
        ""                           # options (booleans)
        "NAME;TEST_SOURCE;ASM_SOURCE" # one-value keywords
        "RENAME_SYMS;LIBS;INCLUDE_DIRS;ASM_INCLUDE_DIRS" # multi-value keywords
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
    add_test(NAME ${ARG_NAME} COMMAND ${ARG_NAME})

    # ── ASM equivalence (when enabled) ────────────────────────────────────
    if(LBA2_BUILD_ASM_TESTS AND ARG_ASM_SOURCE)
        target_compile_definitions(${ARG_NAME} PRIVATE LBA2_ASM_TESTS)

        # Assemble the ASM file ------------------------------------------
        get_filename_component(_asm_name ${ARG_ASM_SOURCE} NAME_WE)
        get_filename_component(_asm_dir  ${ARG_ASM_SOURCE} DIRECTORY)
        set(_asm_obj "${CMAKE_CURRENT_BINARY_DIR}/${_asm_name}.asm.o")
        set(_asm_obj_renamed "${CMAKE_CURRENT_BINARY_DIR}/${_asm_name}.asm.renamed.o")

        # Patch .model SMALL → .model FLAT so the ASM produces a valid
        # 32-bit ELF object (identical instruction encoding, different
        # relocation metadata).  Files already using FLAT are unchanged.
        set(_asm_src_patched "${CMAKE_CURRENT_BINARY_DIR}/${_asm_name}_flat.ASM")
        add_custom_command(
            OUTPUT ${_asm_src_patched}
            COMMAND ${CMAKE_COMMAND}
                    -DSRC=${ARG_ASM_SOURCE}
                    -DDST=${_asm_src_patched}
                    -P ${CMAKE_SOURCE_DIR}/tests/cmake/patch_asm_flat.cmake
            DEPENDS ${ARG_ASM_SOURCE}
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
    endif()
endfunction()

# ─────────────────────────────────────────────────────────────────────────────
# Convenience: add a CPP-only test (no ASM counterpart)
# ─────────────────────────────────────────────────────────────────────────────
function(add_cpp_test)
    cmake_parse_arguments(ARG "" "NAME;TEST_SOURCE" "LIBS;INCLUDE_DIRS" ${ARGN})

    add_executable(${ARG_NAME} ${ARG_TEST_SOURCE})
    target_include_directories(${ARG_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/LIB386/H
        ${CMAKE_SOURCE_DIR}/tests
        ${ARG_INCLUDE_DIRS}
    )
    target_link_libraries(${ARG_NAME} PRIVATE ${ARG_LIBS})
    target_link_libraries(${ARG_NAME} PRIVATE m)
    add_test(NAME ${ARG_NAME} COMMAND ${ARG_NAME})
endfunction()
