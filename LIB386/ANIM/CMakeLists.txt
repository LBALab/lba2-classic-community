set(SOURCES_FILES_ASM ANIM.ASM BODY.ASM CLEAR.ASM FRAME.ASM INTANIM.ASM INTERDEP.ASM INTFRAME.ASM LIBINIT.ASM
        STOFRAME.ASM TEXTURE.ASM)

add_library(ani ${SOURCES_FILES_ASM})

# Configure all ASM files to be recognized as such (workaround for bug where .ASM uppercase extension is not recognized)
foreach (FILE_ASM IN LISTS SOURCES_FILES_ASM)
    set_source_files_properties(${FILE_ASM} PROPERTIES LANGUAGE ASM_MASM)
endforeach ()
