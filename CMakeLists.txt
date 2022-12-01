cmake_minimum_required(VERSION 3.23)

project(LBA2)

# Available compilation options
set(SOUND_BACKEND "miles" CACHE STRING "Sound backend to be used")

# Options sanitization
## Sound backend
set(SOUND_BACKEND_OPTIONS miles sdl none)
set_property(CACHE SOUND_BACKEND PROPERTY STRINGS ${SOUND_BACKEND_OPTIONS})
if (NOT SOUND_BACKEND IN_LIST SOUND_BACKEND_OPTIONS)
    message(SEND_ERROR "SOUND_BACKEND set with invalid option '${SOUND_BACKEND}'. Valid options are: ${SOUND_BACKEND_OPTIONS}")
endif ()

add_subdirectory(LIB386)
