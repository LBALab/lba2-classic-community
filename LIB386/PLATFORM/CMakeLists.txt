set(PLATFORM_BACKEND "sdl3" CACHE STRING "Platform implementation")
set_property(CACHE PLATFORM_BACKEND PROPERTY STRINGS sdl3 sdl2 null)
if(NOT PLATFORM_BACKEND MATCHES "^(sdl3|sdl2|null)$")
    message(FATAL_ERROR "Unsupported PLATFORM_BACKEND '${PLATFORM_BACKEND}'. Available backends: sdl3, sdl2, null")
endif()

if(PLATFORM_BACKEND STREQUAL "sdl2")
    if(ANDROID)
        message(FATAL_ERROR "PLATFORM_BACKEND=sdl2 supports desktop only; Android requires sdl3.")
    endif()
    # Bounded conversion needs integer-phase resampling: floating phase can
    # change PCM at submission boundaries even for integer rate ratios.
    # See docs/AUDIO.md, "SDL2 and SDL3 output transport".
    find_package(SDL2 2.28.0 REQUIRED CONFIG)
    if(LBA2_LINK_STATIC)
        set(_SDL2_TARGET SDL2::SDL2-static)
    else()
        set(_SDL2_TARGET SDL2::SDL2)
    endif()
    if(NOT TARGET ${_SDL2_TARGET})
        message(FATAL_ERROR "The SDL2 package does not provide ${_SDL2_TARGET}.")
    endif()
    add_library(lba2_sdl2_sdk INTERFACE)
    target_link_libraries(lba2_sdl2_sdk INTERFACE ${_SDL2_TARGET})
    target_compile_definitions(lba2_sdl2_sdk INTERFACE SDL_MAIN_HANDLED)
    add_library(lba2_platform STATIC PLATFORM.CPP SDL2/BACKEND.CPP
        SDL2/EVENTS.CPP SDL2/WINDOW.CPP SDL2/HOST.CPP NULL/ANDROID.CPP)
    if(NINTENDO_SWITCH)
        target_sources(lba2_platform PRIVATE SDL2/SWITCH.c)
        set(_SDL2_ENTRY SDL2/ENTRY_SWITCH.CPP)
    elseif(VITA)
        target_sources(lba2_platform PRIVATE SDL2/VITA.CPP)
        set(_SDL2_ENTRY SDL2/ENTRY_VITA.CPP)
    else()
        set(_SDL2_ENTRY SDL2/ENTRY.CPP)
    endif()
    target_include_directories(lba2_platform PUBLIC ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(lba2_platform PRIVATE lba2_sdl2_sdk)
    if(LBA2_BUILD_TESTS)
        target_compile_definitions(lba2_platform PRIVATE LBA2_SDL2_TESTS=1)
    endif()
    set_target_properties(lba2_platform PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(lba2_platform_entry OBJECT ${_SDL2_ENTRY})
    target_include_directories(lba2_platform_entry PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(lba2_platform_entry PRIVATE lba2_sdl2_sdk)
    if(VITA AND LBA2_VITA_DIAGNOSTICS)
        target_sources(lba2_platform PRIVATE SDL2/VITA_LOG.CPP)
        target_compile_definitions(lba2_platform PRIVATE LBA2_VITA_DIAGNOSTICS=1)
        target_compile_definitions(lba2_platform_entry PRIVATE LBA2_VITA_DIAGNOSTICS=1)
        target_link_libraries(lba2_platform PRIVATE debugnet SceNet_stub SceNetCtl_stub SceSysmodule_stub)
    endif()
    set(LBA2_SDL_TARGET lba2_sdl2_sdk PARENT_SCOPE)
    return()
endif()

if(PLATFORM_BACKEND STREQUAL "null")
    if(NOT SOUND_BACKEND STREQUAL "null")
        message(FATAL_ERROR "PLATFORM_BACKEND=null requires SOUND_BACKEND=null; SDL audio requires an SDL2/SDL3 platform SDK.")
    endif()
    if(ANDROID)
        message(FATAL_ERROR "PLATFORM_BACKEND=null builds a desktop executable, not an Android Activity library.")
    endif()
    find_package(Threads REQUIRED)
    add_library(lba2_platform STATIC PLATFORM.CPP STAGING_BUFFER.CPP
        NULL/BACKEND.CPP NULL/ANDROID.CPP)
    target_include_directories(lba2_platform PUBLIC ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(lba2_platform PRIVATE Threads::Threads)
    if(LBA2_BUILD_TESTS)
        target_compile_definitions(lba2_platform PRIVATE LBA2_NULL_TESTS=1)
    endif()
    set_target_properties(lba2_platform PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(lba2_platform_entry OBJECT NULL/ENTRY.CPP)
    return()
endif()

if(ANDROID)
    find_package(SDL3 REQUIRED CONFIG)
    set(_SDL3_TARGET SDL3::SDL3)
elseif(LBA2_LINK_STATIC)
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-static)
    set(_SDL3_TARGET SDL3::SDL3-static)
    if(NOT TARGET ${_SDL3_TARGET})
        message(FATAL_ERROR
            "LBA2_LINK_STATIC=ON but SDL3::SDL3-static is not available. "
            "Build SDL3 with -DSDL_STATIC=ON or use -DLBA2_LINK_STATIC=OFF.")
    endif()
else()
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
    set(_SDL3_TARGET SDL3::SDL3)
endif()

add_library(lba2_sdl3_sdk INTERFACE)
target_link_libraries(lba2_sdl3_sdk INTERFACE ${_SDL3_TARGET})

set(_PLATFORM_SOURCES
    PLATFORM.CPP
    SDL3/ANDROID.CPP
    STAGING_BUFFER.CPP
    SDL3/BACKEND.CPP
    SDL3/SYSTEM.CPP
    SDL3/EVENTS.CPP
    SDL3/INPUT.CPP
    SDL3/WINDOW.CPP
    SDL3/HOST.CPP)

function(add_sdl3_platform target)
    add_library(${target} STATIC ${_PLATFORM_SOURCES})
    target_include_directories(${target} PUBLIC ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(${target} PRIVATE ${_SDL3_TARGET})
    target_compile_definitions(${target} PRIVATE SDL_DISABLE_OLD_NAMES)
    if(LBA2_BUILD_TESTS)
        target_compile_definitions(${target} PRIVATE LBA2_SDL3_TESTS=1)
    endif()
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

add_sdl3_platform(lba2_platform)
if(LBA2_BUILD_TESTS)
    add_sdl3_platform(lba2_platform_test_staging)
    target_compile_definitions(lba2_platform_test_staging PRIVATE LBA2_SDL3_TEST_STAGING=1)
endif()

# The host owns entry-point conventions (SDL_main on Android), not game logic.
add_library(lba2_platform_entry OBJECT SDL3/ENTRY.CPP)
target_link_libraries(lba2_platform_entry PRIVATE ${_SDL3_TARGET})
set_target_properties(lba2_platform_entry PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The sibling AIL SDL backend and SDL-specific conformance tests own their SDK dependency.
set(LBA2_SDL3_TARGET lba2_sdl3_sdk PARENT_SCOPE)
set(LBA2_SDL_TARGET lba2_sdl3_sdk PARENT_SCOPE)
