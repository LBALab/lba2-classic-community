set(_SVGA_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/SVGA)
set(_SVGA_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)
set(_SVGA_LIBS svg sys poly SDL3::SDL3)

# .model SMALL ASM files are automatically patched to FLAT at build time.
add_asm_cpp_test(NAME test_box        TEST_SOURCE test_box.cpp        ASM_SOURCE ${_SVGA_ASM_DIR}/BOX.ASM        RENAME_SYMS Box                         LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
# CLRBOXF: SetClearColor uses Watcom register calling convention (parm [eax]).
# ClearBoxF is C-adapted with PROC params — callable directly.
add_asm_cpp_test(NAME test_clrboxf    TEST_SOURCE test_clrboxf.cpp    ASM_SOURCE ${_SVGA_ASM_DIR}/CLRBOXF.ASM    RENAME_SYMS ClearBox SetClearColor ClearColor ClearBoxF
    LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_font       TEST_SOURCE test_font.cpp       ASM_SOURCE ${_SVGA_ASM_DIR}/FONT.ASM       RENAME_SYMS SizeFont CarFont Font PtrFont InterLeave InterSpace  LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_affstr     TEST_SOURCE test_affstr.cpp     ASM_SOURCE ${_SVGA_ASM_DIR}/AFFSTR.ASM     RENAME_SYMS AffString SizeChar TextInk TextPaper  LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})

# BLITBOXF.CPP contains only commented ASM — no C implementation, placeholder.
add_asm_cpp_test(NAME test_blitboxf   TEST_SOURCE test_blitboxf.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/BLITBOXF.ASM   RENAME_SYMS BlitBox BlitBoxF                LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})

add_asm_cpp_test(NAME test_calcmask   TEST_SOURCE test_calcmask.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/CALCMASK.ASM   RENAME_SYMS CalcGraphMsk                LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_copymask   TEST_SOURCE test_copymask.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/COPYMASK.ASM   RENAME_SYMS CopyMask                    LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_graph      TEST_SOURCE test_graph.cpp      ASM_SOURCE ${_SVGA_ASM_DIR}/GRAPH.ASM      RENAME_SYMS AffGraph GetBoxGraph         LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_mask       TEST_SOURCE test_mask.cpp       ASM_SOURCE ${_SVGA_ASM_DIR}/MASK.ASM       RENAME_SYMS AffMask                     LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_savblock   TEST_SOURCE test_savblock.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/SAVBLOCK.ASM   RENAME_SYMS SaveBlock                   LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_resblock   TEST_SOURCE test_resblock.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/RESBLOCK.ASM   RENAME_SYMS RestoreBlock                LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_cpyblock   TEST_SOURCE test_cpyblock.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/CPYBLOCK.ASM   RENAME_SYMS CopyBlock                   LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_cpybloci   TEST_SOURCE test_cpybloci.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/CPYBLOCI.ASM   RENAME_SYMS CopyBlockIncrust            LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_scalebox   TEST_SOURCE test_scalebox.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/SCALEBOX.ASM   RENAME_SYMS ScaleBox                    LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_scalespi   TEST_SOURCE test_scalespi.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/SCALESPI.ASM   RENAME_SYMS ScaleSprite                 LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
add_asm_cpp_test(NAME test_scalespt   TEST_SOURCE test_scalespt.cpp   ASM_SOURCE ${_SVGA_ASM_DIR}/SCALESPT.ASM   RENAME_SYMS ScaleSpriteTransp           LIBS ${_SVGA_LIBS}  ASM_INCLUDE_DIRS ${_SVGA_ASM_INC})
