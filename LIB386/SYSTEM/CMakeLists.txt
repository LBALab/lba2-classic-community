# General sources
set(SOURCES_FILES ANDROID.CPP AVAILMEM.CPP CMDLINE.CPP CUE.CPP DEFFILE.CPP DISCIMG.CPP DISPOS.CPP EVENTS.CPP EXIT.CPP
        FILECOPY.CPP FILENAME.CPP FILES.CPP FOCUS_AUDIO.CPP HQFILE.CPP HQMEM.CPP HQRLOAD.CPP HQRMEM.CPP HQRMLOAD.CPP HQRRESS.CPP
        INPUT.CPP ISO9660.CPP ITOA.CPP KEYBOARD.CPP LOADMALL.CPP LOADSAVE.CPP LOGPRINT.CPP LOG.CPP MOUSE.CPP N_MALLOC.CPP
        S_MALLOC.CPP STRING.CPP SYSTEM.CPP TIMER.CPP WINDOW.CPP UTILS.CPP
        OPENXR.CPP)

# set(SOURCES_FILES_ASM CPU.ASM FASTCPYF.ASM LZ.ASM MOUSEDAT.ASM)
set(SOURCES_FILES_ASM CPU.CPP FASTCPYF.CPP LZ.CPP MOUSEDAT.CPP)

add_library(sys ${SOURCES_FILES} ${SOURCES_FILES_ASM})
target_link_libraries(sys PUBLIC svg)
if(GPURENDERER)
    target_compile_definitions(sys PRIVATE USE_GPURENDERER)
endif()

if(LBA2_OPENXR)
    target_compile_definitions(sys PRIVATE LBA2_OPENXR=1)
    target_link_libraries(sys PRIVATE openxr_loader)
    if(ANDROID)
        target_link_libraries(sys PRIVATE EGL log android)
    endif()
endif()

# Configure all ASM files to be recognized as such (workaround for bug where .ASM uppercase extension is not recognized)
# foreach (FILE_ASM IN LISTS SOURCES_FILES_ASM)
#     set_source_files_properties(${FILE_ASM} PROPERTIES LANGUAGE ASM_MASM)
# endforeach ()
