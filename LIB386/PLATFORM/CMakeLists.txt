set(PLATFORM_BACKEND "sdl3" CACHE STRING "Platform implementation")
set_property(CACHE PLATFORM_BACKEND PROPERTY STRINGS sdl3)
if(NOT PLATFORM_BACKEND STREQUAL "sdl3")
    message(FATAL_ERROR "Unsupported PLATFORM_BACKEND '${PLATFORM_BACKEND}'. Available backends: sdl3")
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
    ANDROID.CPP
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
