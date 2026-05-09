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

# Engine version string. Reads the canonical value from the VERSION file
# at the repo root and (optionally) appends -dirty when the working tree
# has uncommitted changes. Produces both ${CMAKE_BINARY_DIR}/VERSION.txt
# (plain text, for release pipelines) and ${CMAKE_BINARY_DIR}/VERSION_GENERATED.h
# (defines LBA2_VERSION_STRING for the binary).
include(${CMAKE_SOURCE_DIR}/cmake/git_version.cmake)

# Product metadata. Single source of truth for executable name, runtime window
# title, .desktop entry, and AppImage labels. Override any of these with -D...
# at configure time (see docs/RELEASING.md).
set(LBA2_EXECUTABLE_NAME    "lba2cc"                        CACHE STRING "Name of the built game executable")
set(LBA2_PRODUCT_NAME       "LBA2 Classic Community"        CACHE STRING "Display name (window title, .desktop Name, AppImage label)")
set(LBA2_PRODUCT_NAME_DEMO  "LBA2 Twinsen's Odyssey Demo"   CACHE STRING "Display name when the binary is built with -DDEMO")
set(LBA2_PRODUCT_DESCRIPTION "A community fork for evolving and modernizing LBA2 source port"
                                                            CACHE STRING "One-line description (.desktop Comment field)")
set(LBA2_DESKTOP_ID         "lba2cc"                        CACHE STRING "Desktop file basename and icon ID")

# Build info header (timestamp from compile, flags from configure,
# version macro from the include of VERSION_GENERATED.h inside BUILD_INFO.h.in).
set(BUILD_FLAGS_STR "SOUND_BACKEND=${SOUND_BACKEND} MVIDEO_BACKEND=${MVIDEO_BACKEND} ENABLE_ASM=${ENABLE_ASM}")
configure_file(${CMAKE_SOURCE_DIR}/SOURCES/BUILD_INFO.h.in ${CMAKE_BINARY_DIR}/BUILD_INFO.h)

# Generate the .desktop entry and AppImage env file from the product metadata
# above. Outputs land in ${CMAKE_BINARY_DIR}/packaging/ — make-appimage.sh
# sources them.
configure_file(${CMAKE_SOURCE_DIR}/packaging/lba2cc.desktop.in
               ${CMAKE_BINARY_DIR}/packaging/${LBA2_DESKTOP_ID}.desktop @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/packaging/appimage_env.sh.in
               ${CMAKE_BINARY_DIR}/packaging/appimage_env.sh @ONLY)

# Process the targets
add_subdirectory(LIB386)
add_subdirectory(SOURCES)
add_subdirectory(tools)

if(LBA2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
