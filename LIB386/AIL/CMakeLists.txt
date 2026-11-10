add_library(ail)

if (${SOUND_BACKEND} STREQUAL "null")
    add_subdirectory(NULL)
elseif (${SOUND_BACKEND} STREQUAL "miles")
    add_subdirectory(MILES)
elseif (${SOUND_BACKEND} STREQUAL "sdl" OR ${SOUND_BACKEND} STREQUAL "xaudio2")
    # xaudio2: the same sound rules over an XAudio2 transport, for the UWP build.
    add_subdirectory(SDL)
endif ()
