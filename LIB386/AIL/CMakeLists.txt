add_library(ail)

if (${SOUND_BACKEND} STREQUAL "null")
    add_subdirectory(NULL)
elseif (${SOUND_BACKEND} STREQUAL "miles")
    add_subdirectory(MILES)
elseif (${SOUND_BACKEND} STREQUAL "sdl")
    add_subdirectory(SDL)
endif ()

# Smacker video needs SDL video/audio (StartVideoAudio, PushVideoAudio, etc.). When sound
# backend is not SDL, the SDL subdirectory is not built, so add VIDEO_AUDIO_SDL here.
if (MVIDEO_BACKEND STREQUAL "smacker" AND NOT SOUND_BACKEND STREQUAL "sdl")
    target_sources(ail PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/SDL/VIDEO_AUDIO_SDL.CPP)
    target_link_libraries(ail PRIVATE SDL3::SDL3)
endif ()
