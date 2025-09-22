add_library(ail)

if (${SOUND_BACKEND} STREQUAL "null")
    add_subdirectory(NULL)
elseif (${SOUND_BACKEND} STREQUAL "miles")
    add_subdirectory(MILES)
endif ()
