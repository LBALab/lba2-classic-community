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
# libsmacker comes before LIB386/H, as in the game target, so <smacker.h> finds
# libsmacker's header and not LIB386/H/SMACKER.H on a case-insensitive filesystem.
target_include_directories(test_music_pause_resume PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES/CONSOLE
    ${CMAKE_SOURCE_DIR}/LIB386/libsmacker
    ${CMAKE_SOURCE_DIR}/LIB386/H
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

# Pure path formatting used by the legacy SDL AIL CD interface; no SDL SDK.
add_executable(test_cd_track_path test_cd_track_path.cpp)
set_target_properties(test_cd_track_path PROPERTIES CXX_STANDARD 11)
add_test(NAME test_cd_track_path COMMAND test_cd_track_path)
register_host_test(test_cd_track_path)

# Where the SDL music stream resumes after a pause: the real STREAM.CPP against a
# fake audio device (the AILAudio_* seam) with a virtual clock. Needs SDL for its
# types, SDL_Log and the OGG decode thread, but never opens a device or reads a
# game asset. fixtures/silence-10s.ogg is 10 s of silence, 44.1 kHz stereo, made with
# ffmpeg -f lavfi -i anullsrc=r=44100:cl=stereo -t 10 -c:a vorbis -strict -2.
if(PLATFORM_BACKEND MATCHES "^sdl[23]$")
    add_executable(test_music_stream_resume test_music_stream_resume.cpp
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/STREAM.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/stb_vorbis.c
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CDDA.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CDTRACKS.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CUE.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DISCIMG.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ISO9660.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FILES.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP)
    target_include_directories(test_music_stream_resume PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(test_music_stream_resume PRIVATE ${LBA2_SDL_TARGET})
    lba2_test_tmp_dir(_tmp test_music_stream_resume)
    lba2_test_device_data(tests/music/fixtures/silence-10s.ogg)
    target_compile_definitions(test_music_stream_resume PRIVATE LBA_LOG_NO_PLATFORM
        LBA2_STREAM_TESTS
        TEST_TMP_DIR="${_tmp}"
        TEST_FIXTURE_OGG="${LBA2_TEST_DATA_ROOT}/tests/music/fixtures/silence-10s.ogg")
    if(PLATFORM_BACKEND STREQUAL "sdl2")
        target_compile_definitions(test_music_stream_resume PRIVATE LBA2_USE_SDL2=1)
    endif()
    set_target_properties(test_music_stream_resume PROPERTIES CXX_STANDARD 11)
    add_test(NAME test_music_stream_resume COMMAND test_music_stream_resume)
    register_host_test(test_music_stream_resume)
endif()
