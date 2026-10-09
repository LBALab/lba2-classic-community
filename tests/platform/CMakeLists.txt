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

add_executable(test_platform_contract
    test_platform_contract.cpp
    test_platform_header.c
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
add_test(NAME test_platform_contract COMMAND test_platform_contract)
register_host_test(test_platform_contract)

if(PLATFORM_BACKEND STREQUAL "sdl3")
    foreach(kind sdl3 sdl3_staging)
        set(target test_platform_${kind})
        add_executable(${target} test_platform_sdl3.cpp ${_PLATFORM_FACADES})
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
