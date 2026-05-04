add_subdirectory(3DEXT)

# TODO: For Windows target, Compile resource file "lba2.rc" or reuse "lba2.res"

set(EMBEDDED_LBA2_CFG_CPP ${CMAKE_BINARY_DIR}/SOURCES/embedded_lba2_cfg_data.cpp)
add_custom_command(
    OUTPUT ${EMBEDDED_LBA2_CFG_CPP}
    COMMAND ${CMAKE_COMMAND} "-DINPUT=${CMAKE_SOURCE_DIR}/SOURCES/LBA2.CFG"
            "-DOUTPUT=${EMBEDDED_LBA2_CFG_CPP}" -P
            ${CMAKE_SOURCE_DIR}/cmake/embed_lba2_cfg.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/SOURCES/LBA2.CFG ${CMAKE_SOURCE_DIR}/cmake/embed_lba2_cfg.cmake
    COMMENT "Embedding SOURCES/LBA2.CFG for default config fallback"
)

# General sources
set(SOURCES_FILES AMBIANCE.CPP ANIMTEX.CPP BEZIER.CPP BUILD_INFO.CPP BUGGY.CPP CHEATCOD.CPP COMPORTE.CPP CONFIG.CPP CREDITS.CPP
        DART.CPP DIRECTORIES.CPP DISKFUNC.CPP EMBEDDED_CFG_WRITE.CPP EXTFUNC.CPP EXTRA.CPP FICHE.CPP FLOW.CPP GAMEMENU.CPP GAMEMENU_MOUSE.CPP GERELIFE.CPP
        GERETRAK.CPP GLOBAL.CPP GRILLE.CPP HOLOBODY.CPP HOLOGLOB.CPP HOLOPLAN.CPP IMPACT.CPP INCRUST.CPP INITADEL.C
        INPUT.CPP INTEXT.CPP INVENT.CPP LZSS.CPP MEM.CPP MESSAGE.CPP MUSIC.CPP OBJECT.CPP PATCH.CPP PERSO.CPP
        RES_DISCOVERY.CPP
        PLAYACF.CPP POF.CPP RAIN.CPP SAVEGAME.CPP SAVEGAME_LOAD_BOUNDS.CPP SCAN.CPP SORT.CPP VALIDPOS.CPP VERSION.CPP WAGON.CPP ZV.CPP)
# set(SOURCES_FILES_ASM COMPRESS.ASM FIRE.ASM FLOW_A.ASM FUNC.ASM GRILLE_A.ASM PLASMA.ASM)
#set(SOURCES_FILES_ASM_EXCLUDED COPY.ASM)
set(SOURCES_FILES_ASM COMPRESS.CPP FIRE.CPP FLOW_A.CPP FUNC.CPP GRILLE_A.CPP PLASMA.CPP)

# Windows specific sources
set(SOURCES_FILES_WIN JOYSTICK.CPP)

# DOS specific sources
set(SOURCES_FILES_DOS CRITICAL.CPP HERCULE.CPP)
set(SOURCES_FILES_ASM_DOS DEC_XCF.ASM HERCUL_A.ASM KEYB.ASM)

list(APPEND SOURCES_FILES CONSOLE/CONSOLE_CMD.CPP)
add_executable(lba2 ${SOURCES_FILES} ${SOURCES_FILES_ASM} ${SOURCES_FILES_WIN} ${EMBEDDED_LBA2_CFG_CPP})
# GCC LTO mis-infers T_PLASMA::data_start size when ptrc writes into Malloc tail (see ANIMTEX.CPP plasma case).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(ANIMTEX.CPP PROPERTIES COMPILE_OPTIONS "-Wno-stringop-overflow")
  target_link_options(lba2 PRIVATE "-Wno-stringop-overflow")
endif()
add_subdirectory(CONSOLE)
target_link_libraries(lba2 PRIVATE console)
target_include_directories(lba2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CONSOLE)
# Explicit order matters for GNU static linking: poly references RoundType/ROUND in libsys (UTILS.CPP);
# sys must follow poly. (Transitive deps from 3dext alone order sys before poly and break -m32 links.)
target_link_libraries(lba2 PRIVATE 3dext 3D ail ani fio obj poly svg sys SDL3::SDL3)
if (WIN32)
    target_link_libraries(lba2 PRIVATE gdi32 user32 ddraw winmm kernel32)
endif ()

# Optional: polygon recording support for ASM-vs-CPP testing
# (option declared in top-level CMakeLists.txt)
if(ENABLE_POLY_RECORDING)
  target_link_libraries(lba2 PRIVATE snapshot)
  target_compile_definitions(lba2 PRIVATE ENABLE_POLY_RECORDING)
endif()

# Put libsmacker before LIB386/H so <smacker.h> resolves to libsmacker on case-insensitive FS (e.g. macOS)
target_include_directories(lba2 PRIVATE
        ${CMAKE_BINARY_DIR}
        $<$<STREQUAL:${MVIDEO_BACKEND},smacker>:${CMAKE_SOURCE_DIR}/LIB386/libsmacker>
        ../LIB386/H)
target_compile_definitions(lba2 PRIVATE ONE_GAME_DIRECTORY LBA_GAME CDROM
        $<$<STREQUAL:${MVIDEO_BACKEND},null>:USE_MVIDEO_NULL>
        $<$<STREQUAL:${MVIDEO_BACKEND},smacker>:USE_MVIDEO_SMACKER>
        SDL_DISABLE_OLD_NAMES)
if(MVIDEO_BACKEND STREQUAL "smacker")
  target_link_libraries(lba2 PRIVATE libsmacker video_audio_resample)
endif()

# Configure all ASM files to be recognized as such (workaround for bug where .ASM uppercase extension is not recognized)
# foreach (FILE_ASM IN LISTS SOURCES_FILES_ASM SOURCES_FILES_ASM_DOS)
#     set_source_files_properties(${FILE_ASM} PROPERTIES LANGUAGE ASM_MASM)
# endforeach ()

# Source files in folder but not used on CMakeScript:
# - DEC.ASM
# - MESSTWIN.CPP (Commented on source file "GERELIFE.CPP")
