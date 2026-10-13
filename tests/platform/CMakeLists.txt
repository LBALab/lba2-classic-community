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
        register_host_test(${target})
    endforeach()
endif()

if(PLATFORM_BACKEND STREQUAL "sdl2")
    add_executable(test_platform_sdl2 test_platform_sdl2.cpp)
    target_include_directories(test_platform_sdl2 PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_compile_definitions(test_platform_sdl2 PRIVATE LBA2_SDL2_TESTS=1)
    target_link_libraries(test_platform_sdl2 PRIVATE lba2_platform ${LBA2_SDL_TARGET})
    set_target_properties(test_platform_sdl2 PROPERTIES CXX_STANDARD 11)
    add_test(NAME test_platform_sdl2 COMMAND test_platform_sdl2)
    register_host_test(test_platform_sdl2)
endif()

if(PLATFORM_BACKEND MATCHES "^sdl[23]$")
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
