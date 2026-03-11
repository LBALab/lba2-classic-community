set(_ANIM_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/ANIM)
set(_ANIM_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)
set(_ANIM_LIBS ani obj 3D poly svg sys SDL3::SDL3)

# .model SMALL ASM files are automatically patched to FLAT at build time.
add_asm_cpp_test(NAME test_clear      TEST_SOURCE test_clear.cpp      ASM_SOURCE ${_ANIM_ASM_DIR}/CLEAR.ASM      RENAME_SYMS ObjectClear              LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
add_asm_cpp_test(NAME test_body       TEST_SOURCE test_body.cpp       ASM_SOURCE ${_ANIM_ASM_DIR}/BODY.ASM       RENAME_SYMS ObjectInitBody           LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
# TEXTURE.ASM: proc uses Watcom register calling convention (parm [ebx] [eax]);
# can't call from C without an ABI wrapper — CPP-only.
add_cpp_test(NAME test_texture    TEST_SOURCE test_texture.cpp    LIBS ${_ANIM_LIBS})
add_asm_cpp_test(NAME test_anim       TEST_SOURCE test_anim.cpp       ASM_SOURCE ${_ANIM_ASM_DIR}/ANIM.ASM       RENAME_SYMS ObjectInitAnim           LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
add_asm_cpp_test(NAME test_frame      TEST_SOURCE test_frame.cpp      ASM_SOURCE ${_ANIM_ASM_DIR}/FRAME.ASM      RENAME_SYMS ObjectSetFrame           LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
add_asm_cpp_test(NAME test_intanim    TEST_SOURCE test_intanim.cpp    ASM_SOURCE ${_ANIM_ASM_DIR}/INTANIM.ASM    RENAME_SYMS ObjectSetInterAnim       LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
add_asm_cpp_test(NAME test_interdep   TEST_SOURCE test_interdep.cpp   ASM_SOURCE ${_ANIM_ASM_DIR}/INTERDEP.ASM   RENAME_SYMS ObjectSetInterDep        LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
add_asm_cpp_test(NAME test_intframe   TEST_SOURCE test_intframe.cpp   ASM_SOURCE ${_ANIM_ASM_DIR}/INTFRAME.ASM   RENAME_SYMS ObjectSetInterFrame      LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
add_asm_cpp_test(NAME test_stoframe   TEST_SOURCE test_stoframe.cpp   ASM_SOURCE ${_ANIM_ASM_DIR}/STOFRAME.ASM   RENAME_SYMS ObjectStoreFrame         LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
add_asm_cpp_test(NAME test_libinit    TEST_SOURCE test_libinit.cpp    ASM_SOURCE ${_ANIM_ASM_DIR}/LIBINIT.ASM    RENAME_SYMS InitObjects ClearObjects Lib3DBufferAnim EndLib3DBufferAnim PtrLib3DBufferAnim TransFctAnim  LIBS ${_ANIM_LIBS}  ASM_INCLUDE_DIRS ${_ANIM_ASM_INC})
