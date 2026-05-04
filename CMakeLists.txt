cmake_minimum_required(VERSION 3.23)

project(LBA2)

# Option to enable x86 assembly (requires UASM). Default OFF since all ASM is ported to C++.
option(ENABLE_ASM "Enable x86 assembly (requires UASM assembler)" OFF)

if(ENABLE_ASM)
    # UASM assembly compiler setup (must happen before enable_language)
    set(CMAKE_ASM_MASM_COMPILER uasm)
    set(CMAKE_ASM_MASM_FLAGS_INIT "-D_WIN32 -Zm -c -Cx -Cp -W4 -nologo -Sg -Sa -Zd")
    if(WIN32)
        set(CMAKE_ASM_MASM_FLAGS_INIT ${CMAKE_ASM_MASM_FLAGS_INIT}" -coff")
    else()
        set(CMAKE_ASM_MASM_FLAGS_INIT ${CMAKE_ASM_MASM_FLAGS_INIT}" -elf -zcw")
    endif()
    enable_language(C CXX ASM_MASM)
else()
    enable_language(C CXX)
endif()

set(CMAKE_CXX_STANDARD 98)

# Compilation options
set(SOUND_BACKEND "sdl" CACHE STRING "Sound backend to be used")
set(MVIDEO_BACKEND "smacker" CACHE STRING "Motion Video backend to be used")
option(CONSOLE_MODULE "Enable Quake-style debug console (backtick/F12)" OFF)

# Compilation options sanitization
## Sound backend
set(SOUND_BACKEND_OPTIONS null miles sdl)
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

# Testing options
option(LBA2_BUILD_TESTS "Build test suites" OFF)
option(LBA2_BUILD_ASM_EQUIV_TESTS "Build ASM↔CPP equivalence tests (requires objcopy; use OFF for host-only discovery tests)" ON)
option(ENABLE_POLY_RECORDING "Enable polygon call recording for ASM-vs-CPP testing" OFF)

# Debug tools (original Adeline developer debug features)
option(DEBUG_TOOLS "Enable developer debug tools (collision viz, benchmarks, bug saves)" OFF)
if(DEBUG_TOOLS)
    add_compile_definitions(DEBUG_TOOLS=1)
endif()

# Performance options
option(LBA2_NATIVE_ARCH "Use -march=native for host-optimized build (not portable)" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "LTO not supported: ${ipo_error}")
    endif()
endif()

if(LBA2_NATIVE_ARCH)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64|i[3-6]86")
        add_compile_options(-march=native)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        add_compile_options(-mcpu=native)
    endif()
endif()

# Dependencies
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

# Build info header (timestamp from compile, flags from configure)
set(BUILD_FLAGS_STR "CONSOLE_MODULE=${CONSOLE_MODULE} SOUND_BACKEND=${SOUND_BACKEND} MVIDEO_BACKEND=${MVIDEO_BACKEND} ENABLE_ASM=${ENABLE_ASM}")
configure_file(${CMAKE_SOURCE_DIR}/SOURCES/BUILD_INFO.h.in ${CMAKE_BINARY_DIR}/BUILD_INFO.h)

# Process the targets
add_subdirectory(LIB386)
add_subdirectory(SOURCES)
add_subdirectory(tools)

if(LBA2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
