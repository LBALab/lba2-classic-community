set(_3D_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/3D)
set(_3D_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

# NOTE: Most 3D/ ASM files use .model SMALL which produces segmented code
# incompatible with flat 32-bit Linux ELF.  Only PRLI3DF.ASM uses .model FLAT.
# All SMALL-model tests are CPP-only; FLAT-model tests include ASM equivalence.

# ── Data table tests (SMALL model → CPP-only) ────────────────────────────
add_cpp_test(NAME test_sintab   TEST_SOURCE test_sintab.cpp   LIBS 3D sys)
add_cpp_test(NAME test_sintabf  TEST_SOURCE test_sintabf.cpp  LIBS 3D sys)
add_cpp_test(NAME test_tantab   TEST_SOURCE test_tantab.cpp   LIBS 3D sys)

# ── Pure math tests (SMALL model → CPP-only) ─────────────────────────────
add_cpp_test(NAME test_getang2d  TEST_SOURCE test_getang2d.cpp  LIBS 3D sys)
add_cpp_test(NAME test_getang3d  TEST_SOURCE test_getang3d.cpp  LIBS 3D sys)
add_cpp_test(NAME test_sqrroot   TEST_SOURCE test_sqrroot.cpp   LIBS 3D sys)
add_cpp_test(NAME test_regle3    TEST_SOURCE test_regle3.cpp    LIBS 3D sys)

# ── Matrix tests (SMALL model → CPP-only) ────────────────────────────────
add_cpp_test(NAME test_copymatf   TEST_SOURCE test_copymatf.cpp   LIBS 3D sys)
add_cpp_test(NAME test_imatstdf   TEST_SOURCE test_imatstdf.cpp   LIBS 3D sys)
add_cpp_test(NAME test_imattraf   TEST_SOURCE test_imattraf.cpp   LIBS 3D sys)
add_cpp_test(NAME test_mulmatf    TEST_SOURCE test_mulmatf.cpp    LIBS 3D sys)
add_cpp_test(NAME test_rotmatu    TEST_SOURCE test_rotmatu.cpp    LIBS 3D sys)

# ── Rotation & transform tests (SMALL model → CPP-only) ──────────────────
add_cpp_test(NAME test_lrot2df    TEST_SOURCE test_lrot2df.cpp    LIBS 3D sys)
add_cpp_test(NAME test_lrot3df    TEST_SOURCE test_lrot3df.cpp    LIBS 3D sys)
add_cpp_test(NAME test_lirot3df   TEST_SOURCE test_lirot3df.cpp   LIBS 3D sys)
add_cpp_test(NAME test_rotralif   TEST_SOURCE test_rotralif.cpp   LIBS 3D sys)
add_cpp_test(NAME test_rotvect    TEST_SOURCE test_rotvect.cpp    LIBS 3D sys)

# ── Projection tests (SMALL model → CPP-only, except PRLI3DF which is FLAT) ─
add_cpp_test(NAME test_proj       TEST_SOURCE test_proj.cpp       LIBS 3D sys)
add_cpp_test(NAME test_lproj3df   TEST_SOURCE test_lproj3df.cpp   LIBS 3D sys)
add_cpp_test(NAME test_lprojiso   TEST_SOURCE test_lprojiso.cpp   LIBS 3D sys)

# PRLI3DF uses .model FLAT but has position-dependent code (DT_TEXTREL) that
# segfaults in PIE mode. CPP-only for now.
add_cpp_test(NAME test_prli3df  TEST_SOURCE test_prli3df.cpp  LIBS 3D sys)

add_cpp_test(NAME test_prliiso    TEST_SOURCE test_prliiso.cpp    LIBS 3D sys)

# LITLISTF has cross-module ASM deps — CPP-only
add_cpp_test(NAME test_litlistf   TEST_SOURCE test_litlistf.cpp   LIBS 3D sys)
