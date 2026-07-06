# Host regression test for the MUSIC.CPP pause/resume contract.
# Compiles the real MUSIC.CPP (with CDROM, as the shipping game does) and the
# real RegleTrois/BoundRegleTrois; the test file provides a spy AIL backend.
#
# MUSIC.CPP includes the engine-wide C_EXTERN.H, whose header chain needs the
# same include dirs/defines as the shipping SOURCES target. None of them pull
# SDL headers (MUSIC.CPP's object has no SDL symbols), so the test stays a
# self-contained host build: no SDL, no audio device, no game assets.
add_executable(test_music_pause_resume
    test_music_pause_resume.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/MUSIC.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/3D/REGLE3.CPP
)

# NB: SOURCES is intentionally NOT an include dir. MUSIC.CPP reaches its own
# headers (C_EXTERN.H ...) via current-file-dir resolution; putting SOURCES on
# the -I path shadows a LIB386/H header in that chain and breaks the parse
# (matches how the shipping lba2cc target compiles MUSIC.CPP). The test file
# therefore declares the few SOURCES-side symbols itself.
target_include_directories(test_music_pause_resume PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES/CONSOLE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/LIB386/libsmacker
    ${CMAKE_BINARY_DIR}
)

# Match the shipping game's defines so we test the real (CDROM) code path.
target_compile_definitions(test_music_pause_resume PRIVATE
    CDROM
    LBA_GAME
    ONE_GAME_DIRECTORY
    SDL_DISABLE_OLD_NAMES
    USE_MVIDEO_SMACKER
    USE_SOUND_SDL
)

set_target_properties(test_music_pause_resume PROPERTIES CXX_STANDARD 11)

add_test(NAME test_music_pause_resume COMMAND test_music_pause_resume)

register_host_test(test_music_pause_resume)

# Host unit test for the stream park/resume + focus-suspend decision logic
# (LIB386/H/AIL/STREAM_PARK.H). Pure and SDL-free: no backend, no audio device, no assets.
add_executable(test_music_stream_park test_music_stream_park.cpp)

target_include_directories(test_music_stream_park PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

set_target_properties(test_music_stream_park PROPERTIES CXX_STANDARD 11)

add_test(NAME test_music_stream_park COMMAND test_music_stream_park)

register_host_test(test_music_stream_park)
