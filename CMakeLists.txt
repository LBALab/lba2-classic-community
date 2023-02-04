cmake_minimum_required(VERSION 3.23)

project(LBA2)

# Configure languages and compilers
set(CMAKE_ASM_MASM_COMPILER uasm) # TODO: Move to toolchain file
set(CMAKE_ASM_MASM_FLAGS_INIT "-D_WIN32 -Zm -c -Cx -Cp -W4 -nologo -Sg -Sa -Zd") # TODO: Move to toolchain file
if (WIN32)
    set(CMAKE_ASM_MASM_FLAGS_INIT ${CMAKE_ASM_MASM_FLAGS_INIT}" -coff") # TODO: Move to toolchain file
else ()
    set(CMAKE_ASM_MASM_FLAGS_INIT ${CMAKE_ASM_MASM_FLAGS_INIT}" -elf -zcw") # TODO: Move to toolchain file
endif ()
# TODO: Add /Zd only in debug mode

# TODO: Check all uses of structure alignment /zp8 and /zp2 (ASM code also uses ALIGN)
# TODO: Re-check all source files for all folders and list non used ones on CMakeScript
enable_language(C CXX ASM_MASM)
set(CMAKE_CXX_STANDARD 98)

# Compilation options
set(SOUND_BACKEND "miles" CACHE STRING "Sound backend to be used")
set(MVIDEO_BACKEND "smacker" CACHE STRING "Motion Video backend to be used")

# Compilation options sanitization
## Sound backend
set(SOUND_BACKEND_OPTIONS miles none)
set_property(CACHE SOUND_BACKEND PROPERTY STRINGS ${SOUND_BACKEND_OPTIONS})
if (NOT SOUND_BACKEND IN_LIST SOUND_BACKEND_OPTIONS)
    message(SEND_ERROR "SOUND_BACKEND set with invalid option '${SOUND_BACKEND}'. Valid options are: ${SOUND_BACKEND_OPTIONS}")
endif ()
## Motion Video backend
set(MVIDEO_BACKEND_OPTIONS smacker none)
set_property(CACHE MVIDEO_BACKEND PROPERTY STRINGS ${MVIDEO_BACKEND_OPTIONS})
if (NOT MVIDEO_BACKEND IN_LIST MVIDEO_BACKEND_OPTIONS)
    message(SEND_ERROR "MVIDEO_BACKEND set with invalid option '${MVIDEO_BACKEND}'. Valid options are: ${MVIDEO_BACKEND_OPTIONS}")
endif ()

# Dependencies
find_package(SDL2 REQUIRED)

# Process the targets
add_subdirectory(LIB386)
add_subdirectory(SOURCES)
