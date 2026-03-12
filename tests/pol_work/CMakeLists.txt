set(_POLY_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/pol_work)
set(_POLY_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

# pol_work tests are placeholders — the polygon rendering functions have heavy
# state dependencies (fill tables, edge buffers, Z-buffer, screen).
# Full tests require substantial fixture setup.

add_asm_cpp_test(NAME test_poly       TEST_SOURCE test_poly.cpp       LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_poly_jmp   TEST_SOURCE test_poly_jmp.cpp   LIBS poly svg sys SDL3::SDL3)
# NOTE: POLYCLIP.ASM references Status_Int/Status_Float from POLY.ASM.
# ASM equivalence requires adding POLY.ASM as ASM_DEP. For now, CPP-only.
add_asm_cpp_test(NAME test_polyclip   TEST_SOURCE test_polyclip.cpp   LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_polydisc   TEST_SOURCE test_polydisc.cpp   LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_polyflat   TEST_SOURCE test_polyflat.cpp   LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_polygour   TEST_SOURCE test_polygour.cpp   LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_polygtex   TEST_SOURCE test_polygtex.cpp   LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_polyline   TEST_SOURCE test_polyline.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYLINE.ASM
    RENAME_SYMS Line LineZBuf LineZBufNZW Line_A
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polytext   TEST_SOURCE test_polytext.cpp   LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_polytexz   TEST_SOURCE test_polytexz.cpp   LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_polytzf    TEST_SOURCE test_polytzf.cpp    LIBS poly svg sys SDL3::SDL3)
add_asm_cpp_test(NAME test_polytzg    TEST_SOURCE test_polytzg.cpp    LIBS poly svg sys SDL3::SDL3)
# TestVuePolyF: C-adapted PROC has "uses eax" which clobbers the return value.
# We need to patch the ASM to remove 'eax' from the 'uses' clause.
# For now, use STRIP_C_ADAPT + inline asm wrapper (Watcom: input in ESI).
add_asm_cpp_test(NAME test_testvuef   TEST_SOURCE test_testvuef.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/TESTVUEF.ASM
    RENAME_SYMS TestVuePolyF TestVuePoly
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC}
    STRIP_C_ADAPT)
