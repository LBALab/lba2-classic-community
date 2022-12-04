cmake_minimum_required(VERSION 3.23)

project(LBA2)

# Configure languages and compilers
set(CMAKE_ASM_MASM_COMPILER uasm) # TODO: Move to toolchain file
set(CMAKE_ASM_MASM_FLAGS_INIT "-D_WIN32 -nologo -Cx -Zm -c -Sa -Zd") # TODO: Move to toolchain file
# TODO: Check if assembler flags for /DREG_CALL or /DSTACK_CALL are necessary and if move to toolchain or target config
enable_language(C CXX ASM_MASM)
set(CMAKE_CXX_STANDARD 98)

# Compilation options
set(SOUND_BACKEND "miles" CACHE STRING "Sound backend to be used")

# Compilation options sanitization
## Sound backend
set(SOUND_BACKEND_OPTIONS miles sdl none)
set_property(CACHE SOUND_BACKEND PROPERTY STRINGS ${SOUND_BACKEND_OPTIONS})
if (NOT SOUND_BACKEND IN_LIST SOUND_BACKEND_OPTIONS)
    message(SEND_ERROR "SOUND_BACKEND set with invalid option '${SOUND_BACKEND}'. Valid options are: ${SOUND_BACKEND_OPTIONS}")
endif ()

add_subdirectory(LIB386)
