add_library(ail)

if (${SOUND_BACKEND} STREQUAL "miles")
    target_sources(ail PRIVATE CD.CPP COMMON.CPP MIDDIG.CPP MIDI.CPP MIXER.CPP SAMPLE.CPP STREAM.CPP TIMER.CPP)
    target_compile_definitions(ail PUBLIC USE_SOUND_MILES)
elseif (${SOUND_BACKEND} STREQUAL "none")
    add_subdirectory(NULL_BACKEND)
endif ()
