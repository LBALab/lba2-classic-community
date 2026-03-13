set(_POLY_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/pol_work)
set(_POLY_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

# pol_work tests are placeholders — the polygon rendering functions have heavy
# state dependencies (fill tables, edge buffers, Z-buffer, screen).
# Full tests require substantial fixture setup.

add_asm_cpp_test(NAME test_poly       TEST_SOURCE test_poly.cpp       LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLY.ASM
    RENAME_SYMS Fill_PolyFast SetScreenPitch SetFog SetCLUT Switch_Fillers_ASM
        Fill_PolyClip Fill_ClipXMin Fill_ClipXMax Fill_ClipYMin Fill_ClipYMax
    ASM_DEPS ${_POLY_ASM_DIR}/POLYFLAT.ASM ${_POLY_ASM_DIR}/POLYDISC.ASM ${_POLY_ASM_DIR}/POLYLINE.ASM
        ${_POLY_ASM_DIR}/POLY_JMP.ASM
    ASM_DEP_RENAME_SYMS
        Triangle_ReadNextEdge
        Filler_Flat Filler_Transparent Filler_Trame
        Filler_FlatZBuf Filler_TransparentZBuf Filler_TrameZBuf
        Filler_FlatNZW Filler_TransparentNZW Filler_TrameNZW
        Filler_FlagZBuffer
        Fill_Sphere
        Line_A Line LineZBuf LineZBufNZW
        Fill_N_Table_Jumps Fill_Fog_Table_Jumps Fill_ZBuf_Table_Jumps
        Fill_FogZBuf_Table_Jumps Fill_NZW_Table_Jumps Fill_FogNZW_Table_Jumps
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
target_link_options(test_poly PRIVATE -Wl,--allow-multiple-definition)
add_asm_cpp_test(NAME test_poly_jmp   TEST_SOURCE test_poly_jmp.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLY_JMP.ASM
    RENAME_SYMS Fill_N_Table_Jumps Fill_Fog_Table_Jumps Fill_ZBuf_Table_Jumps Fill_FogZBuf_Table_Jumps Fill_NZW_Table_Jumps Fill_FogNZW_Table_Jumps
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
target_link_options(test_poly_jmp PRIVATE -Wl,--allow-multiple-definition)
add_asm_cpp_test(NAME test_polyclip   TEST_SOURCE test_polyclip.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYCLIP.ASM
    RENAME_SYMS ClipperZ
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polydisc   TEST_SOURCE test_polydisc.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYDISC.ASM
    RENAME_SYMS Fill_Sphere
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polyflat   TEST_SOURCE test_polyflat.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYFLAT.ASM
    RENAME_SYMS Filler_Flat Filler_Transparent Filler_Trame Filler_FlatZBuf Filler_TransparentZBuf Filler_TrameZBuf Filler_FlatNZW Filler_TransparentNZW Filler_TrameNZW Filler_FlagZBuffer
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polygour   TEST_SOURCE test_polygour.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYGOUR.ASM
    RENAME_SYMS Filler_Gouraud Filler_Dither Filler_GouraudTable Filler_DitherTable Filler_GouraudFog Filler_DitherFog Filler_GouraudZBuf Filler_DitherZBuf Filler_GouraudTableZBuf Filler_DitherTableZBuf Filler_GouraudFogZBuf Filler_DitherFogZBuf Filler_GouraudNZW Filler_DitherNZW Filler_GouraudTableNZW Filler_DitherTableNZW Filler_GouraudFogNZW Filler_DitherFogNZW
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polygtex   TEST_SOURCE test_polygtex.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYGTEX.ASM
    RENAME_SYMS Filler_TextureGouraud Filler_TextureDither Filler_TextureGouraudChromaKey Filler_TextureDitherChromaKey Filler_TextureGouraudZBuf Filler_TextureGouraudChromaKeyZBuf Filler_TextureGouraudNZW Filler_TextureGouraudChromaKeyNZW
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polyline   TEST_SOURCE test_polyline.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYLINE.ASM
    RENAME_SYMS Line LineZBuf LineZBufNZW Line_A
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polytext   TEST_SOURCE test_polytext.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYTEXT.ASM
    RENAME_SYMS Filler_Texture Filler_TextureFlat Filler_TextureChromaKey Filler_TextureFlatChromaKey Filler_TextureFog Filler_TextureChromaKeyFog Filler_TextureZBuf Filler_TextureChromaKeyZBuf Filler_TextureFlatZBuf Filler_TextureFlatChromaKeyZBuf Filler_TextureFogZBuf Filler_TextureChromaKeyFogZBuf Filler_TextureNZW Filler_TextureChromaKeyNZW Filler_TextureFlatNZW Filler_TextureFlatChromaKeyNZW Filler_TextureFogNZW Filler_TextureChromaKeyFogNZW
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polytexz   TEST_SOURCE test_polytexz.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYTEXZ.ASM
    RENAME_SYMS Filler_TextureZ Filler_TextureZFlat Filler_TextureZChromaKey Filler_TextureZFlatChromaKey Filler_TextureZFog Filler_TextureZChromaKeyFog Filler_TextureZZBuf Filler_TextureZChromaKeyZBuf Filler_TextureZFlatZBuf Filler_TextureZFlatChromaKeyZBuf Filler_TextureZFogZBuf Filler_TextureZChromaKeyFogZBuf Filler_TextureZNZW Filler_TextureZChromaKeyNZW Filler_TextureZFlatNZW Filler_TextureZFlatChromaKeyNZW Filler_TextureZFogNZW Filler_TextureZChromaKeyFogNZW Fill_Init_Perspective Fill_Loop_Perspective
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polytzf   TEST_SOURCE test_polytzf.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYTZF.ASM
    RENAME_SYMS Filler_TextureZFogSmooth Filler_TextureZFogSmoothZBuf Filler_TextureZFogSmoothNZW Fill_Init_Perspective Fill_Loop_Perspective
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
add_asm_cpp_test(NAME test_polytzg   TEST_SOURCE test_polytzg.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/POLYTZG.ASM
    RENAME_SYMS Filler_TextureZGouraud Filler_TextureZGouraudChromaKey Filler_TextureZGouraudZBuf Filler_TextureZGouraudChromaKeyZBuf Filler_TextureZGouraudNZW Filler_TextureZGouraudChromaKeyNZW
    ASM_DEPS ${_POLY_ASM_DIR}/POLYTEXZ.ASM
    ASM_DEP_RENAME_SYMS Fill_Init_Perspective Fill_Loop_Perspective
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC})
target_link_options(test_polytzg PRIVATE -Wl,--allow-multiple-definition)
# TestVuePolyF: C-adapted PROC has "uses eax" which clobbers the return value.
# We need to patch the ASM to remove 'eax' from the 'uses' clause.
# For now, use STRIP_C_ADAPT + inline asm wrapper (Watcom: input in ESI).
add_asm_cpp_test(NAME test_testvuef   TEST_SOURCE test_testvuef.cpp   LIBS poly svg sys SDL3::SDL3
    ASM_SOURCE ${_POLY_ASM_DIR}/TESTVUEF.ASM
    RENAME_SYMS TestVuePolyF TestVuePoly
    ASM_INCLUDE_DIRS ${_POLY_ASM_INC}
    STRIP_C_ADAPT)
