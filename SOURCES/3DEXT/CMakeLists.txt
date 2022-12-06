# TODO Add Flags to C compiler:
# -i=D:\COMPIL\WATCOM\h;D:\COMPIL\WATCOM\h\nt -fh -w4 -e25 -d_WIN32 -zq -otexan -d1 -zld -5r -bt=nt -mf

# TODO For ASM, check flags differences and apply what is necessary
# LBA2WIN: *wasm <PATH_TO_FILE> -mf -5r -w4 -e25 -zq
# LBA2:    ml /Zd /W3 /nologo /D_WIN32 /c /Cx /Zm /Sa /DNoLanguage=SYSCALL /Djumps=; <PATH_TO_FILE>

# General sources
set(SOURCES_FILES DECORS.CPP GLOBEXT.CPP LOADISLE.CPP MAPTOOLS.CPP TERRAIN.CPP)
set(SOURCES_FILES_ASM BOXZBUF.ASM LINERAIN.ASM)

add_library(3dext ${SOURCES_FILES} ${SOURCES_FILES_ASM})
target_compile_definitions(3dext PRIVATE ONE_GAME_DIRECTORY LBA_GAME CDROM)
target_link_libraries(3dext PUBLIC sys svg 3D obj poly men)
target_include_directories(3dext PUBLIC ../../LIB386/H)

# Configure all ASM files to be recognized as such (workaround for bug where .ASM uppercase extension is not recognized)
foreach (FILE_ASM IN LISTS SOURCES_FILES_ASM)
    set_source_files_properties(${FILE_ASM} PROPERTIES LANGUAGE ASM_MASM)
endforeach ()

# Source files in folder but not used on CMakeScript:
# - DRAWSKY.CPP (but used as included on source file "TERRAIN.CPP")
# - VAR_EXT.CPP (but used as included on source file "GLOBEXT.CPP")
