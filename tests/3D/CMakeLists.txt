set(_3D_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/3D)
set(_3D_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

# ── Data table tests (CPP-only: compare table values) ─────────────────────
add_asm_cpp_test(
    NAME test_sintab  TEST_SOURCE test_sintab.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/SINTAB.ASM
    RENAME_SYMS SinTab CosTab
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_sintabf  TEST_SOURCE test_sintabf.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/SINTABF.ASM
    RENAME_SYMS SinTabF CosTabF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_tantab  TEST_SOURCE test_tantab.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/TANTAB.ASM
    RENAME_SYMS TanTab
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

# ── Pure math tests ───────────────────────────────────────────────────────
add_asm_cpp_test(
    NAME test_getang2d  TEST_SOURCE test_getang2d.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/GETANG2D.ASM
    RENAME_SYMS GetAngleVector2D
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_getang3d  TEST_SOURCE test_getang3d.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/GETANG3D.ASM
    RENAME_SYMS GetAngleVector3D
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_sqrroot  TEST_SOURCE test_sqrroot.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/SQRROOT.ASM
    RENAME_SYMS Sqr QSqr
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_regle3  TEST_SOURCE test_regle3.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/REGLE3.ASM
    RENAME_SYMS RegleTrois BoundRegleTrois
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

# ── Matrix operation tests ────────────────────────────────────────────────
add_asm_cpp_test(
    NAME test_copymatf  TEST_SOURCE test_copymatf.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/COPYMATF.ASM
    RENAME_SYMS CopyMatrixF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_imatstdf  TEST_SOURCE test_imatstdf.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/IMATSTDF.ASM
    RENAME_SYMS InitMatrixStdF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_imattraf  TEST_SOURCE test_imattraf.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/IMATTRAF.ASM
    RENAME_SYMS InitMatrixTransF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_mulmatf  TEST_SOURCE test_mulmatf.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/MULMATF.ASM
    RENAME_SYMS MulMatrixF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_rotmatu  TEST_SOURCE test_rotmatu.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/ROTMATU.ASM
    RENAME_SYMS RotateMatrixU
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

# ── Rotation & transform tests ────────────────────────────────────────────
add_asm_cpp_test(
    NAME test_lrot2df  TEST_SOURCE test_lrot2df.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/LROT2DF.ASM
    RENAME_SYMS LongRotate Rotate
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_lrot3df  TEST_SOURCE test_lrot3df.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/LROT3DF.ASM
    RENAME_SYMS LongRotatePointF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_lirot3df  TEST_SOURCE test_lirot3df.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/LIROT3DF.ASM
    RENAME_SYMS LongInverseRotatePointF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_rotralif  TEST_SOURCE test_rotralif.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/ROTRALIF.ASM
    RENAME_SYMS RotTransListF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_rotvect  TEST_SOURCE test_rotvect.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/ROTVECT.ASM
    RENAME_SYMS RotateVector
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

# ── Projection tests ──────────────────────────────────────────────────────
add_asm_cpp_test(
    NAME test_proj  TEST_SOURCE test_proj.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/PROJ.ASM
    RENAME_SYMS SetProjection SetIsoProjection
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_lproj3df  TEST_SOURCE test_lproj3df.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/LPROJ3DF.ASM
    RENAME_SYMS LongProjectPoint3D
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_lprojiso  TEST_SOURCE test_lprojiso.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/LPROJISO.ASM
    RENAME_SYMS LongProjectPointIso
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_prli3df  TEST_SOURCE test_prli3df.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/PRLI3DF.ASM
    RENAME_SYMS ProjectList3DF
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_prliiso  TEST_SOURCE test_prliiso.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/PRLIISO.ASM
    RENAME_SYMS ProjectListIso
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)

add_asm_cpp_test(
    NAME test_litlistf  TEST_SOURCE test_litlistf.cpp
    ASM_SOURCE ${_3D_ASM_DIR}/LITLISTF.ASM
    RENAME_SYMS LightList
    LIBS 3D sys
    ASM_INCLUDE_DIRS ${_3D_ASM_INC}
)
