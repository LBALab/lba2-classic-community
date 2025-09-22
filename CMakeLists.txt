cmake_minimum_required(VERSION 3.23)

project(LBA2)

# Bruteforce setup of UASM assembly compiler (must happen before enable_language)
set(CMAKE_ASM_MASM_COMPILER uasm)
set(CMAKE_ASM_MASM_FLAGS_INIT "-D_WIN32 -Zm -c -Cx -Cp -W4 -nologo -Sg -Sa -Zd")
if (WIN32)
    set(CMAKE_ASM_MASM_FLAGS_INIT ${CMAKE_ASM_MASM_FLAGS_INIT}" -coff")
else ()
    set(CMAKE_ASM_MASM_FLAGS_INIT ${CMAKE_ASM_MASM_FLAGS_INIT}" -elf -zcw")
endif ()

enable_language(C CXX ASM_MASM)
set(CMAKE_CXX_STANDARD 98)

# Compilation options
set(SOUND_BACKEND "null" CACHE STRING "Sound backend to be used")
set(MVIDEO_BACKEND "null" CACHE STRING "Motion Video backend to be used")

# Compilation options sanitization
## Sound backend
set(SOUND_BACKEND_OPTIONS null miles)
set_property(CACHE SOUND_BACKEND PROPERTY STRINGS ${SOUND_BACKEND_OPTIONS})
if (NOT SOUND_BACKEND IN_LIST SOUND_BACKEND_OPTIONS)
    message(SEND_ERROR "SOUND_BACKEND set with invalid option '${SOUND_BACKEND}'. Valid options are: ${SOUND_BACKEND_OPTIONS}")
endif ()
## Motion Video backend
set(MVIDEO_BACKEND_OPTIONS null smacker)
set_property(CACHE MVIDEO_BACKEND PROPERTY STRINGS ${MVIDEO_BACKEND_OPTIONS})
if (NOT MVIDEO_BACKEND IN_LIST MVIDEO_BACKEND_OPTIONS)
    message(SEND_ERROR "MVIDEO_BACKEND set with invalid option '${MVIDEO_BACKEND}'. Valid options are: ${MVIDEO_BACKEND_OPTIONS}")
endif ()

# Dependencies
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

# Process the targets
add_subdirectory(LIB386)
add_subdirectory(SOURCES)
