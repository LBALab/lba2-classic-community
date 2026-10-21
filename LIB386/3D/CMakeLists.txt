# General sources
set(SOURCES_FILES CAMERA.CPP DATAMAT.CPP DISTANCE.CPP IMATSTD.CPP LIGHT.CPP LIROT3D.CPP LPROJ.CPP LROT3D.CPP MOVE.CPP
        PROJREC.CPP ROT3D.CPP)

# set(SOURCES_FILES_ASM COPYMATF.ASM GETANG2D.ASM GETANG3D.ASM IMATSTDF.ASM IMATSTDI.ASM IMATTRAF.ASM LIGHT.ASM
#         LIROT3DF.ASM LIROT3DI.ASM LITLISTF.ASM LPROJ3DF.ASM LPROJ3DI.ASM LPROJISO.ASM LROT2DF.ASM LROT3DF.ASM
#         LROT3DI.ASM MULMATF.ASM PRLI3DF.ASM PRLIISO.ASM PROJ.ASM REGLE3.ASM ROTMATU.ASM ROTRALIF.ASM ROTVECT.ASM
#         SINTAB.ASM SINTABF.ASM SQRROOT.ASM TANTAB.ASM)
set(SOURCES_FILES_ASM COPYMATF.CPP GETANG2D.CPP GETANG3D.CPP IMATSTDF.CPP IMATSTDI.CPP IMATTRAF.CPP LIGHT.CPP
        LIROT3DF.CPP LIROT3DI.CPP LITLISTF.CPP LPROJ3DF.CPP LPROJ3DI.CPP LPROJISO.CPP LROT2DF.CPP LROT3DF.CPP
        LROT3DI.CPP MULMATF.CPP PRLI3DF.CPP PRLIISO.CPP PROJ.CPP REGLE3.CPP ROTMATU.CPP ROTRALIF.CPP ROTVECT.CPP
        SINTAB.CPP SINTABF.CPP SQRROOT.CPP TANTAB.CPP)

add_library(3D ${SOURCES_FILES} ${SOURCES_FILES_ASM})

# The float routines round with an inline add of 1.5 * 2^52 where long double is
# not the x87's (3D/X87REAL.H). GCC's default -ffp-contract=fast fuses a multiply
# into that add (one rounding instead of two), which moves exact ties; seen on
# PowerPC, and aarch64 Linux builds with GCC too. Clang contracts only inside one
# expression and is unaffected, but takes the same flag. x86 has no x87 FMA.
# tests/numeric_contract and tests/precision_paths compile some of these files
# themselves and repeat the flag.
target_compile_options(3D PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-ffp-contract=off>)

# Configure all ASM files to be recognized as such (workaround for bug where .ASM uppercase extension is not recognized)
# foreach (FILE_ASM IN LISTS SOURCES_FILES_ASM)
#     set_source_files_properties(${FILE_ASM} PROPERTIES LANGUAGE ASM_MASM)
# endforeach ()
