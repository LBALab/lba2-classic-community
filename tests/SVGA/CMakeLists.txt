set(_SVGA_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/SVGA)
set(_SVGA_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)
set(_SVGA_LIBS svg sys poly SDL3::SDL3)

# SMALL-model ASM → CPP-only tests
add_cpp_test(NAME test_box        TEST_SOURCE test_box.cpp        LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_clrboxf    TEST_SOURCE test_clrboxf.cpp    LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_font       TEST_SOURCE test_font.cpp       LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_affstr     TEST_SOURCE test_affstr.cpp     LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_blitboxf   TEST_SOURCE test_blitboxf.cpp   LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_calcmask   TEST_SOURCE test_calcmask.cpp   LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_copymask   TEST_SOURCE test_copymask.cpp   LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_graph      TEST_SOURCE test_graph.cpp      LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_mask       TEST_SOURCE test_mask.cpp       LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_savblock   TEST_SOURCE test_savblock.cpp   LIBS ${_SVGA_LIBS})
add_cpp_test(NAME test_resblock   TEST_SOURCE test_resblock.cpp   LIBS ${_SVGA_LIBS})

# FLAT-model ASM → ASM equivalence tests possible
add_asm_cpp_test(NAME test_cpyblock  TEST_SOURCE test_cpyblock.cpp  ASM_SOURCE ${_SVGA_ASM_DIR}/CPYBLOCK.ASM  RENAME_SYMS CopyBlock  LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_cpybloci  TEST_SOURCE test_cpybloci.cpp  ASM_SOURCE ${_SVGA_ASM_DIR}/CPYBLOCI.ASM  RENAME_SYMS CopyBlockIncrust  LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_scalebox  TEST_SOURCE test_scalebox.cpp  ASM_SOURCE ${_SVGA_ASM_DIR}/SCALEBOX.ASM  RENAME_SYMS ScaleBox  LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_scalespi  TEST_SOURCE test_scalespi.cpp  ASM_SOURCE ${_SVGA_ASM_DIR}/SCALESPI.ASM  RENAME_SYMS ScaleSprite  LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_scalespt  TEST_SOURCE test_scalespt.cpp  ASM_SOURCE ${_SVGA_ASM_DIR}/SCALESPT.ASM  RENAME_SYMS ScaleSpriteTransp  LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
