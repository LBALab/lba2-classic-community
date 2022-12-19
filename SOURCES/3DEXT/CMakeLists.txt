# General sources
set(SOURCES_FILES DECORS.CPP GLOBEXT.CPP LBA_EXT.CPP LOADISLE.CPP MAPTOOLS.CPP TERRAIN.CPP)
set(SOURCES_FILES_ASM BOXZBUF.ASM LINERAIN.ASM)

add_library(3dext ${SOURCES_FILES} ${SOURCES_FILES_ASM})
target_compile_definitions(3dext PRIVATE ONE_GAME_DIRECTORY LBA_GAME CDROM)
target_link_libraries(3dext PUBLIC sys svg 3D obj poly men ani fio)
target_include_directories(3dext PUBLIC ../../LIB386/H)

# Configure all ASM files to be recognized as such (workaround for bug where .ASM uppercase extension is not recognized)
foreach (FILE_ASM IN LISTS SOURCES_FILES_ASM)
    set_source_files_properties(${FILE_ASM} PROPERTIES LANGUAGE ASM_MASM)
endforeach ()

# Source files in folder but not used on CMakeScript:
# - DRAWSKY.CPP (but used as included on source file "TERRAIN.CPP")
# - VAR_EXT.CPP (but used as included on source file "GLOBEXT.CPP")
