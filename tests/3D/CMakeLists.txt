set(_3D_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/3D)
set(_3D_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

# All tests are ASM-vs-CPP equivalence.  .model SMALL ASM files are
# automatically patched to .model FLAT at build time (see patch_asm_flat.cmake).
#
# Many 3D/ ASM procs use Watcom register calling convention
# (#pragma aux parm [edi] [esi] [ebx]) instead of C stack parameters.
# These SEGFAULT when called from C because the parameters end up on the
# stack instead of in registers.  A few procs with proper PROC parameter
# declarations (GETANG2D, SQRROOT, REGLE3) do work.

# ── Data tables ───────────────────────────────────────────────────────────
add_asm_cpp_test(NAME test_sintab   TEST_SOURCE test_sintab.cpp   ASM_SOURCE ${_3D_ASM_DIR}/SINTAB.ASM   RENAME_SYMS SinTab CosTab                                     LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_sintabf  TEST_SOURCE test_sintabf.cpp  ASM_SOURCE ${_3D_ASM_DIR}/SINTABF.ASM  RENAME_SYMS SinTabF CosTabF                                    LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_tantab   TEST_SOURCE test_tantab.cpp   ASM_SOURCE ${_3D_ASM_DIR}/TANTAB.ASM   RENAME_SYMS TanTab                                             LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})

# ── Pure math ─────────────────────────────────────────────────────────────
add_asm_cpp_test(NAME test_getang2d  TEST_SOURCE test_getang2d.cpp  ASM_SOURCE ${_3D_ASM_DIR}/GETANG2D.ASM  RENAME_SYMS GetAngleVector2D                                LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_getang3d  TEST_SOURCE test_getang3d.cpp  ASM_SOURCE ${_3D_ASM_DIR}/GETANG3D.ASM  RENAME_SYMS GetAngleVector3D                                LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_sqrroot   TEST_SOURCE test_sqrroot.cpp   ASM_SOURCE ${_3D_ASM_DIR}/SQRROOT.ASM   RENAME_SYMS Sqr QSqr                                        LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_regle3    TEST_SOURCE test_regle3.cpp    ASM_SOURCE ${_3D_ASM_DIR}/REGLE3.ASM    RENAME_SYMS RegleTrois BoundRegleTrois                       LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})

# ── Matrix ops ────────────────────────────────────────────────────────────
add_asm_cpp_test(NAME test_copymatf   TEST_SOURCE test_copymatf.cpp   ASM_SOURCE ${_3D_ASM_DIR}/COPYMATF.ASM   RENAME_SYMS CopyMatrix CopyMatrixF                       LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_imatstdf   TEST_SOURCE test_imatstdf.cpp   ASM_SOURCE ${_3D_ASM_DIR}/IMATSTDF.ASM   RENAME_SYMS InitMatrixStd InitMatrix InitMatrixStdF       LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_imattraf   TEST_SOURCE test_imattraf.cpp   ASM_SOURCE ${_3D_ASM_DIR}/IMATTRAF.ASM   RENAME_SYMS InitMatrixTrans InitMatrixTransF               LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_mulmatf    TEST_SOURCE test_mulmatf.cpp    ASM_SOURCE ${_3D_ASM_DIR}/MULMATF.ASM    RENAME_SYMS MulMatrix MulMatrixF                           LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_rotmatu    TEST_SOURCE test_rotmatu.cpp    ASM_SOURCE ${_3D_ASM_DIR}/ROTMATU.ASM    RENAME_SYMS RotateMatrix RotateMatrixU                     LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})

# ── Rotation & transform ─────────────────────────────────────────────────
add_asm_cpp_test(NAME test_lrot2df    TEST_SOURCE test_lrot2df.cpp    ASM_SOURCE ${_3D_ASM_DIR}/LROT2DF.ASM    RENAME_SYMS LongRotate Rotate LongRotateF                  LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_lrot3df    TEST_SOURCE test_lrot3df.cpp    ASM_SOURCE ${_3D_ASM_DIR}/LROT3DF.ASM    RENAME_SYMS LongRotatePoint RotatePoint RotatePointNoMMX LongRotatePointF  LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_lirot3df   TEST_SOURCE test_lirot3df.cpp   ASM_SOURCE ${_3D_ASM_DIR}/LIROT3DF.ASM   RENAME_SYMS LongInverseRotatePoint LongInverseRotatePointF LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_rotralif   TEST_SOURCE test_rotralif.cpp   ASM_SOURCE ${_3D_ASM_DIR}/ROTRALIF.ASM   RENAME_SYMS RotTransList RotTransListF                     LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_rotvect    TEST_SOURCE test_rotvect.cpp    ASM_SOURCE ${_3D_ASM_DIR}/ROTVECT.ASM    RENAME_SYMS RotateVector                                   LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})

# ── Projection ────────────────────────────────────────────────────────────
# PROJ.ASM references LongProjectPoint3DF/ProjectList3DF with C linkage but
# these have C++ linkage in the library — CPP-only
add_cpp_test(NAME test_proj       TEST_SOURCE test_proj.cpp       LIBS 3D sys)
add_asm_cpp_test(NAME test_lproj3df   TEST_SOURCE test_lproj3df.cpp   ASM_SOURCE ${_3D_ASM_DIR}/LPROJ3DF.ASM   RENAME_SYMS LongProjectPoint3D LongProjectPoint LongProjectPoint3DF  LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_lprojiso   TEST_SOURCE test_lprojiso.cpp   ASM_SOURCE ${_3D_ASM_DIR}/LPROJISO.ASM   RENAME_SYMS LongProjectPointIso                            LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_prli3df    TEST_SOURCE test_prli3df.cpp    ASM_SOURCE ${_3D_ASM_DIR}/PRLI3DF.ASM    RENAME_SYMS ProjectList3D ProjectList ProjectList3DF        LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})
add_asm_cpp_test(NAME test_prliiso    TEST_SOURCE test_prliiso.cpp    ASM_SOURCE ${_3D_ASM_DIR}/PRLIISO.ASM    RENAME_SYMS ProjectListIso                                 LIBS 3D sys  ASM_INCLUDE_DIRS ${_3D_ASM_INC})

# ── Lighting (ASM has cross-module dep on LongInverseRotatePointF which has
#    C++ linkage — can't link ASM object, so CPP-only build but test code
#    still has equivalence checks gated behind LBA2_ASM_TESTS)
add_cpp_test(NAME test_litlistf   TEST_SOURCE test_litlistf.cpp   LIBS 3D sys)
