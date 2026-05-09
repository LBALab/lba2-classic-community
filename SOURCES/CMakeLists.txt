add_subdirectory(3DEXT)

# Windows resource file: icon + VERSIONINFO. Generated under
# ${CMAKE_BINARY_DIR}/packaging/ by configure_file in the root CMakeLists.txt.
if(WIN32)
    enable_language(RC)
endif()

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
set(SOURCES_FILES AMBIANCE.CPP ANIMTEX.CPP BEZIER.CPP BUILD_INFO.CPP BUGGY.CPP CHEATCOD.CPP COMPORTE.CPP CONFIG.CPP CREDITS.CPP CREDITS_PARSE.CPP
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
if(WIN32)
    list(APPEND SOURCES_FILES_WIN ${CMAKE_BINARY_DIR}/packaging/lba2cc.rc)
endif()

# On macOS, the .icns icon goes inside the .app's Contents/Resources/ via
# the MACOSX_PACKAGE_LOCATION property below. Adding it to the source list
# makes CMake aware of the dependency so the .app rebuilds when the icon
# is regenerated.
set(SOURCES_FILES_MACOS "")
if(APPLE)
    list(APPEND SOURCES_FILES_MACOS
         ${CMAKE_BINARY_DIR}/packaging/${LBA2_DESKTOP_ID}.icns)
endif()

add_executable(${LBA2_EXECUTABLE_NAME} ${SOURCES_FILES} ${SOURCES_FILES_ASM} ${SOURCES_FILES_WIN} ${SOURCES_FILES_MACOS} ${EMBEDDED_LBA2_CFG_CPP})

# macOS .app bundle metadata: marks the target as a bundled .app, points
# at the configured Info.plist, and routes the .icns to Contents/Resources/.
# Without MACOSX_BUNDLE the binary would be a bare executable in
# build/SOURCES/, not a launchable .app.
if(APPLE)
    set_target_properties(${LBA2_EXECUTABLE_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/packaging/Info.plist
        OUTPUT_NAME ${LBA2_EXECUTABLE_NAME})
    set_source_files_properties(
        ${CMAKE_BINARY_DIR}/packaging/${LBA2_DESKTOP_ID}.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_dependencies(${LBA2_EXECUTABLE_NAME} lba2_icns)

    # Ad-hoc codesign the .app post-build. Apple Silicon (macOS 11+) refuses
    # to launch unsigned binaries — Gatekeeper reports the app as "damaged"
    # and offers only "Move to Bin" as a remedy, even though the binary is
    # fine. The `-` identity is an ad-hoc self-attestation (no Apple
    # Developer account required); it gets the .app past the on-load
    # signature check while we wait for real Developer ID + notarization
    # to land. --deep covers any nested frameworks (none today, but
    # future-proof for when SDL3 might ship as a bundled .dylib).
    add_custom_command(TARGET ${LBA2_EXECUTABLE_NAME} POST_BUILD
        COMMAND codesign --force --deep --sign -
                $<TARGET_BUNDLE_DIR:${LBA2_EXECUTABLE_NAME}>
        COMMENT "Ad-hoc codesigning ${LBA2_EXECUTABLE_NAME}.app"
        VERBATIM)
endif()
# GCC LTO mis-infers T_PLASMA::data_start size when ptrc writes into Malloc tail (see ANIMTEX.CPP plasma case).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(ANIMTEX.CPP PROPERTIES COMPILE_OPTIONS "-Wno-stringop-overflow")
  target_link_options(${LBA2_EXECUTABLE_NAME} PRIVATE "-Wno-stringop-overflow")
endif()
add_subdirectory(CONSOLE)
target_link_libraries(${LBA2_EXECUTABLE_NAME} PRIVATE console)
target_include_directories(${LBA2_EXECUTABLE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CONSOLE)
# Explicit order matters for GNU static linking: poly references RoundType/ROUND in libsys (UTILS.CPP);
# sys must follow poly. (Transitive deps from 3dext alone order sys before poly and break -m32 links.)
target_link_libraries(${LBA2_EXECUTABLE_NAME} PRIVATE 3dext 3D ail ani fio obj poly svg sys ${LBA2_SDL3_TARGET})
if (WIN32)
    target_link_libraries(${LBA2_EXECUTABLE_NAME} PRIVATE gdi32 user32 ddraw winmm kernel32)
    if(LBA2_LINK_STATIC AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Fold the GCC C++ runtime into the binary so the released .exe has
        # no MSYS2/MinGW DLL dependencies (libstdc++-6.dll, libgcc_s_seh-1.dll,
        # libwinpthread-1.dll). Combined with -DLBA2_LINK_STATIC=ON also
        # statically linking SDL3, the result is a single self-contained .exe.
        target_link_options(${LBA2_EXECUTABLE_NAME} PRIVATE
            -static-libstdc++ -static-libgcc -static)
    endif()
endif ()

# Optional: polygon recording support for ASM-vs-CPP testing
# (option declared in top-level CMakeLists.txt)
if(ENABLE_POLY_RECORDING)
  target_link_libraries(${LBA2_EXECUTABLE_NAME} PRIVATE snapshot)
  target_compile_definitions(${LBA2_EXECUTABLE_NAME} PRIVATE ENABLE_POLY_RECORDING)
endif()

# Ensure VERSION_GENERATED.h is up-to-date before any TU including
# BUILD_INFO.h is compiled. The lba2_version_gen target is declared in
# cmake/git_version.cmake and runs the write_version.cmake script.
add_dependencies(${LBA2_EXECUTABLE_NAME} lba2_version_gen)

# Put libsmacker before LIB386/H so <smacker.h> resolves to libsmacker on case-insensitive FS (e.g. macOS)
target_include_directories(${LBA2_EXECUTABLE_NAME} PRIVATE
        ${CMAKE_BINARY_DIR}
        $<$<STREQUAL:${MVIDEO_BACKEND},smacker>:${CMAKE_SOURCE_DIR}/LIB386/libsmacker>
        ../LIB386/H)
target_compile_definitions(${LBA2_EXECUTABLE_NAME} PRIVATE ONE_GAME_DIRECTORY LBA_GAME CDROM
        $<$<STREQUAL:${MVIDEO_BACKEND},null>:USE_MVIDEO_NULL>
        $<$<STREQUAL:${MVIDEO_BACKEND},smacker>:USE_MVIDEO_SMACKER>
        SDL_DISABLE_OLD_NAMES)
if(MVIDEO_BACKEND STREQUAL "smacker")
  target_link_libraries(${LBA2_EXECUTABLE_NAME} PRIVATE libsmacker video_audio_resample)
endif()

# Configure all ASM files to be recognized as such (workaround for bug where .ASM uppercase extension is not recognized)
# foreach (FILE_ASM IN LISTS SOURCES_FILES_ASM SOURCES_FILES_ASM_DOS)
#     set_source_files_properties(${FILE_ASM} PROPERTIES LANGUAGE ASM_MASM)
# endforeach ()

# Source files in folder but not used on CMakeScript:
# - DEC.ASM
# - MESSTWIN.CPP (Commented on source file "GERELIFE.CPP")
