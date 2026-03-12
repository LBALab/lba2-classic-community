set(SOURCES_FILES POLY.CPP)

# set(SOURCES_FILES_ASM POLY.ASM POLY_JMP.ASM POLYCLIP.ASM POLYDISC.ASM POLYFLAT.ASM POLYGOUR.ASM
#         POLYGTEX.ASM POLYLINE.ASM POLYLINE.ASM POLYTEXT.ASM POLYTEXT.ASM POLYTEXZ.ASM POLYTZF.ASM POLYTZG.ASM
#         TESTVUEF.ASM)
set(SOURCES_FILES_ASM POLY.CPP POLY_JMP.CPP POLYCLIP.CPP POLYDISC.CPP POLYFLAT.CPP POLYGOUR.CPP
        POLYGTEX.CPP POLYLINE.CPP POLYLINE.CPP POLYTEXT.CPP POLYTEXT.CPP POLYTEXZ.CPP POLYTZF.CPP POLYTZG.CPP
        TESTVUEF.CPP)

add_library(poly ${SOURCES_FILES} ${SOURCES_FILES_ASM})

# Force x87 FPU math for perspective-correct texture code.
# The ASM uses 80-bit extended precision (fild/fdivr/fmul/fistp) and
# the CPP must match via long double + fistp-style rounding.
set_source_files_properties(POLYTEXZ.CPP PROPERTIES COMPILE_FLAGS "-mfpmath=387 -ffloat-store")
set_source_files_properties(POLYTZF.CPP PROPERTIES COMPILE_FLAGS "-mfpmath=387 -ffloat-store")

# Configure all ASM files to be recognized as such (workaround for bug where .ASM uppercase extension is not recognized)
# foreach (FILE_ASM IN LISTS SOURCES_FILES_ASM)
#     set_source_files_properties(${FILE_ASM} PROPERTIES LANGUAGE ASM_MASM)
# endforeach ()
