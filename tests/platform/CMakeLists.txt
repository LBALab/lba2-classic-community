set(_PLATFORM_FACADES
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/EVENTS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/KEYBOARD.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/MOUSE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/TIMER.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/WINDOW.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FOCUS_AUDIO.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOGPRINT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SVGA/SDL.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/JOYSTICK.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/TOUCH_INPUT.CPP)

add_library(test_platform_header_cpp98 OBJECT test_platform_header_cpp98.cpp)
target_include_directories(test_platform_header_cpp98 PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
set_target_properties(test_platform_header_cpp98 PROPERTIES
    CXX_STANDARD 98 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

# Derive the checks from the contract so a newly added callback cannot be omitted.
set(_PLATFORM_HEADER "${CMAKE_SOURCE_DIR}/LIB386/H/PLATFORM/PLATFORM.H")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_PLATFORM_HEADER}")
file(READ "${_PLATFORM_HEADER}" _platform_contract)
set(_callback_checks "")
foreach(group System Events Window Input Host)
    string(TOLOWER "${group}" member)
    string(REGEX MATCH "typedef struct Platform${group} \\{[^}]*\\}" block "${_platform_contract}")
    string(REGEX MATCHALL "\\(\\*[A-Za-z0-9_]+\\)" callbacks "${block}")
    if(NOT callbacks)
        message(FATAL_ERROR "No callbacks found in Platform${group}")
    endif()
    foreach(callback IN LISTS callbacks)
        string(REGEX REPLACE "\\(\\*([A-Za-z0-9_]+)\\)" "\\1" callback "${callback}")
        string(APPEND _callback_checks "PLATFORM_CALLBACK(${member}.${callback})\n")
    endforeach()
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/platform_callback_checks.inc" "${_callback_checks}")

add_executable(test_platform_callbacks test_platform_callbacks.cpp)
target_include_directories(test_platform_callbacks PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(test_platform_callbacks PRIVATE lba2_platform)
set_target_properties(test_platform_callbacks PROPERTIES CXX_STANDARD 11)
add_test(NAME test_platform_callbacks COMMAND test_platform_callbacks)
register_host_test(test_platform_callbacks)

add_executable(test_platform_contract
    test_platform_contract.cpp
    test_platform_header.c
    $<TARGET_OBJECTS:test_platform_header_cpp98>
    ${CMAKE_SOURCE_DIR}/LIB386/PLATFORM/PLATFORM.CPP
    ${_PLATFORM_FACADES})
target_include_directories(test_platform_contract PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES)
target_compile_definitions(test_platform_contract PRIVATE LBA_LOG_NO_PLATFORM)
set_target_properties(test_platform_contract PROPERTIES CXX_STANDARD 11 C_STANDARD 11)
if(UNIX)
    target_link_libraries(test_platform_contract PRIVATE m)
endif()

if(PLATFORM_BACKEND STREQUAL "null")
    add_executable(test_platform_null test_platform_null.cpp)
    target_include_directories(test_platform_null PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_compile_definitions(test_platform_null PRIVATE LBA2_NULL_TESTS=1)
    target_link_libraries(test_platform_null PRIVATE lba2_platform)
    set_target_properties(test_platform_null PROPERTIES CXX_STANDARD 11)
    add_test(NAME test_platform_null COMMAND test_platform_null)
    register_host_test(test_platform_null)
    if(APPLE)
        set(_NULL_GAME "$<TARGET_BUNDLE_CONTENT_DIR:${LBA2_EXECUTABLE_NAME}>/MacOS/${LBA2_EXECUTABLE_NAME}")
    else()
        set(_NULL_GAME "$<TARGET_FILE:${LBA2_EXECUTABLE_NAME}>")
    endif()
    add_test(NAME test_platform_null_game COMMAND ${CMAKE_COMMAND}
        "-DGAME=${_NULL_GAME}" "-DRUN_DIR=${CMAKE_CURRENT_BINARY_DIR}/null-boot"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_null_boot.cmake)
    set_tests_properties(test_platform_null_game PROPERTIES LABELS host_quick)
    add_dependencies(host_tests ${LBA2_EXECUTABLE_NAME})
endif()
add_test(NAME test_platform_contract COMMAND test_platform_contract)
register_host_test(test_platform_contract)

if(PLATFORM_BACKEND STREQUAL "sdl3")
    foreach(kind sdl3 sdl3_staging)
        set(target test_platform_${kind})
        add_executable(${target} test_platform_sdl3.cpp ${_PLATFORM_FACADES}
            ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/COMMON.CPP)
        target_include_directories(${target} PRIVATE
            ${CMAKE_SOURCE_DIR}/LIB386/H ${CMAKE_SOURCE_DIR}/SOURCES)
        target_compile_definitions(${target} PRIVATE LBA_LOG_NO_PLATFORM LBA2_SDL3_TESTS=1)
        set_target_properties(${target} PROPERTIES CXX_STANDARD 11)
        if(kind STREQUAL "sdl3")
            target_link_libraries(${target} PRIVATE lba2_platform ${LBA2_SDL3_TARGET})
        else()
            target_link_libraries(${target} PRIVATE lba2_platform_test_staging ${LBA2_SDL3_TARGET})
        endif()

        add_test(NAME ${target} COMMAND ${target})
        lba2_test_needs_android_activity(${target})
        register_host_test(${target})
    endforeach()
endif()

if(PLATFORM_BACKEND STREQUAL "sdl2")
    add_executable(test_switch_host test_switch_host.cpp
        ${CMAKE_SOURCE_DIR}/LIB386/PLATFORM/SDL2/SWITCH.c)
    target_include_directories(test_switch_host PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/switch_stub)
    target_link_libraries(test_switch_host PRIVATE lba2_platform ${LBA2_SDL_TARGET})
    set_target_properties(test_switch_host PROPERTIES CXX_STANDARD 11 C_STANDARD 11)
    add_test(NAME test_switch_host COMMAND test_switch_host)
    register_host_test(test_switch_host)

    add_executable(test_platform_sdl2 test_platform_sdl2.cpp)
    target_include_directories(test_platform_sdl2 PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_compile_definitions(test_platform_sdl2 PRIVATE LBA2_SDL2_TESTS=1)
    target_link_libraries(test_platform_sdl2 PRIVATE lba2_platform ${LBA2_SDL_TARGET})
    set_target_properties(test_platform_sdl2 PROPERTIES CXX_STANDARD 11)
    add_test(NAME test_platform_sdl2 COMMAND test_platform_sdl2)
    register_host_test(test_platform_sdl2)
endif()

if(PLATFORM_BACKEND MATCHES "^sdl[23]$")
    add_executable(test_sdl_audio_playback test_sdl_audio_playback.cpp
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/AUDIO_DEVICE.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/COMMON.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/STREAM.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/VIDEO_AUDIO_SDL.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/stb_vorbis.c
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CDDA.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CDTRACKS.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/CUE.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DISCIMG.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ISO9660.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FILES.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP)
    target_include_directories(test_sdl_audio_playback PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(test_sdl_audio_playback PRIVATE ${LBA2_SDL_TARGET})
    lba2_test_tmp_dir(_tmp test_sdl_audio_playback)
    target_compile_definitions(test_sdl_audio_playback PRIVATE LBA_LOG_NO_PLATFORM
        TEST_TMP_DIR="${_tmp}")
    if(PLATFORM_BACKEND STREQUAL "sdl2")
        target_compile_definitions(test_sdl_audio_playback PRIVATE LBA2_USE_SDL2=1)
    endif()
    set_target_properties(test_sdl_audio_playback PROPERTIES CXX_STANDARD 11)
    add_test(NAME test_sdl_audio_playback COMMAND test_sdl_audio_playback)
    register_host_test(test_sdl_audio_playback)

    add_executable(test_sdl_audio test_sdl_audio.cpp
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/AUDIO_DEVICE.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/COMMON.CPP)
    target_include_directories(test_sdl_audio PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(test_sdl_audio PRIVATE ${LBA2_SDL_TARGET})
    target_compile_definitions(test_sdl_audio PRIVATE LBA2_AUDIO_TESTS=1)
    if(PLATFORM_BACKEND STREQUAL "sdl2")
        target_compile_definitions(test_sdl_audio PRIVATE LBA2_USE_SDL2=1)
    endif()
    set_target_properties(test_sdl_audio PROPERTIES CXX_STANDARD 11)
    add_test(NAME test_sdl_audio COMMAND test_sdl_audio)
    register_host_test(test_sdl_audio)
    if(PLATFORM_BACKEND STREQUAL "sdl2")
        add_executable(test_sdl_audio_device test_sdl_audio.cpp
            ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/AUDIO_DEVICE.CPP
            ${CMAKE_SOURCE_DIR}/LIB386/AIL/SDL/COMMON.CPP)
        target_include_directories(test_sdl_audio_device PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
        target_link_libraries(test_sdl_audio_device PRIVATE ${LBA2_SDL_TARGET})
        target_compile_definitions(test_sdl_audio_device PRIVATE LBA2_USE_SDL2=1)
        set_target_properties(test_sdl_audio_device PROPERTIES CXX_STANDARD 11)
        add_test(NAME test_sdl_audio_device COMMAND test_sdl_audio_device)
        register_host_test(test_sdl_audio_device)
    endif()
endif()
