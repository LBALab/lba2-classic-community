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

# Release packaging — static-link SDL3 and the GCC runtime so the binary
# has no DLL dependencies beyond OS libraries. Used by the Windows release
# workflow (single-exe portable ZIP). Default OFF; dev workflows keep the
# faster shared-link path.
option(LBA2_LINK_STATIC "Static-link SDL3 and the GCC C++ runtime (release packaging)" OFF)

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
if(LBA2_LINK_STATIC)
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-static)
    # Be explicit: SDL3::SDL3 is an alias that resolves to the shared variant
    # when both shared and static are present in the install tree (the MSYS2
    # mingw-w64-ucrt-x86_64-SDL3 package ships both). Force the static target
    # so the resulting binary actually has SDL3 folded in.
    set(LBA2_SDL3_TARGET SDL3::SDL3-static)
    if(NOT TARGET ${LBA2_SDL3_TARGET})
        message(FATAL_ERROR
            "LBA2_LINK_STATIC=ON but SDL3::SDL3-static is not available. "
            "Your SDL3 install only provides the shared variant. On MSYS2 "
            "the package mingw-w64-ucrt-x86_64-SDL3 normally includes both; "
            "if it doesn't on your platform, build SDL3 from source with "
            "-DSDL_STATIC=ON or skip -DLBA2_LINK_STATIC=ON.")
    endif()
else()
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
    set(LBA2_SDL3_TARGET SDL3::SDL3)
endif()

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
set(LBA2_BUNDLE_IDENTIFIER  "org.lbalab.lba2cc"             CACHE STRING "macOS .app bundle reverse-DNS identifier")

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

# Windows resource file: drives the .exe icon and Properties → Details
# (Product Name, File Description, Product Version, ...). Compiled into
# the binary on Windows builds via SOURCES/CMakeLists.txt; harmlessly
# generated and ignored on non-Windows.
#
# Configure-time substitution: re-run cmake to refresh after VERSION or
# git dirty-state changes. Release builds (CI on tag) run on fresh
# checkouts so this is invisible there; for local dev it just means a
# stale "Properties → Version" until reconfigure.
file(READ "${CMAKE_SOURCE_DIR}/VERSION" _LBA2_VER_RAW)
string(STRIP "${_LBA2_VER_RAW}" LBA2_VERSION_STRING_CONFIG)
if(GIT_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/.git"
                  AND NOT LBA2_VERSION_STRING_CONFIG MATCHES "-dirty$")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _LBA2_GIT_DIRTY
        OUTPUT_QUIET ERROR_QUIET)
    if(_LBA2_GIT_DIRTY EQUAL 1)
        set(LBA2_VERSION_STRING_CONFIG "${LBA2_VERSION_STRING_CONFIG}-dirty")
    endif()
endif()
# 4-tuple of integers for VERSIONINFO FILEVERSION / PRODUCTVERSION:
# strip pre-release/dirty suffix, split on '.', right-pad zeros to 4 parts.
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _LBA2_VER_NUM "${LBA2_VERSION_STRING_CONFIG}")
string(REPLACE "." ";" _LBA2_VER_PARTS "${_LBA2_VER_NUM}")
list(LENGTH _LBA2_VER_PARTS _LBA2_VER_N)
while(_LBA2_VER_N LESS 4)
    list(APPEND _LBA2_VER_PARTS 0)
    math(EXPR _LBA2_VER_N "${_LBA2_VER_N} + 1")
endwhile()
list(JOIN _LBA2_VER_PARTS "," LBA2_VERSION_TUPLE)
# VS_FF_PRERELEASE (0x2) for any non-pure-numeric version (covers -dev, -dirty,
# -rc1, etc.). Pure x.y.z → clean release (0x0).
if(LBA2_VERSION_STRING_CONFIG MATCHES "[a-zA-Z]")
    set(LBA2_VERSIONINFO_FLAGS "0x2L")
else()
    set(LBA2_VERSIONINFO_FLAGS "0x0L")
endif()
configure_file(${CMAKE_SOURCE_DIR}/packaging/lba2cc.rc.in
               ${CMAKE_BINARY_DIR}/packaging/lba2cc.rc @ONLY)

# macOS .app bundle metadata (Info.plist) + .icns icon generation.
# Only meaningful when building on macOS — gated to avoid generating
# stray files on other platforms.
if(APPLE)
    # Pin the deployment target to 11.0 (Big Sur) — first arm64 macOS,
    # the floor for our universal2-style matrix (arm64 + x86_64). Below
    # this the OS refuses to launch the .app.
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum macOS deployment target")
    endif()

    configure_file(${CMAKE_SOURCE_DIR}/packaging/Info.plist.in
                   ${CMAKE_BINARY_DIR}/packaging/Info.plist @ONLY)

    # Generate <id>.icns from <id>.png at build time using macOS native
    # tools (sips for resampling, iconutil for the multi-resolution
    # container). Both ship with Xcode CLI tools — no extra deps. The
    # .icns is consumed by the .app bundle via CFBundleIconFile in the
    # Info.plist (set to LBA2_DESKTOP_ID, with the .icns extension implied).
    set(_LBA2_ICNS_PNG     "${CMAKE_SOURCE_DIR}/packaging/${LBA2_DESKTOP_ID}.png")
    set(_LBA2_ICNS_OUT     "${CMAKE_BINARY_DIR}/packaging/${LBA2_DESKTOP_ID}.icns")
    set(_LBA2_ICNS_ICONSET "${CMAKE_BINARY_DIR}/packaging/${LBA2_DESKTOP_ID}.iconset")
    add_custom_command(
        OUTPUT ${_LBA2_ICNS_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_LBA2_ICNS_ICONSET}
        COMMAND sips -z 16 16     ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_16x16.png       > /dev/null
        COMMAND sips -z 32 32     ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_16x16@2x.png    > /dev/null
        COMMAND sips -z 32 32     ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_32x32.png       > /dev/null
        COMMAND sips -z 64 64     ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_32x32@2x.png    > /dev/null
        COMMAND sips -z 128 128   ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_128x128.png     > /dev/null
        COMMAND sips -z 256 256   ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_128x128@2x.png  > /dev/null
        COMMAND sips -z 256 256   ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_256x256.png     > /dev/null
        COMMAND sips -z 512 512   ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_256x256@2x.png  > /dev/null
        COMMAND sips -z 512 512   ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_512x512.png     > /dev/null
        COMMAND sips -z 1024 1024 ${_LBA2_ICNS_PNG} --out ${_LBA2_ICNS_ICONSET}/icon_512x512@2x.png  > /dev/null
        COMMAND iconutil --convert icns ${_LBA2_ICNS_ICONSET} --output ${_LBA2_ICNS_OUT}
        DEPENDS ${_LBA2_ICNS_PNG}
        COMMENT "Generating ${LBA2_DESKTOP_ID}.icns from ${LBA2_DESKTOP_ID}.png"
        VERBATIM)
    add_custom_target(lba2_icns ALL DEPENDS ${_LBA2_ICNS_OUT})
endif()

# Process the targets
add_subdirectory(LIB386)
add_subdirectory(SOURCES)
add_subdirectory(tools)

if(LBA2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
