# The binding table and the fold that builds Input from it. Driven through the
# real GetInput(), so LIB386/SYSTEM/INPUT.CPP links alongside; the two keyboard
# hooks are stubbed, which is what keeps SDL and the game stack out. Nothing
# else links: that is what the extraction bought.
add_executable(test_input_bindings
    test_input_bindings.cpp
    held_keys.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP)

# Bindings through the config file, on a temp cfg in the test's own directory.
# SOURCES/INPUT.CPP still holds the polling loop, so this one carries the stubs.
add_executable(test_input_config
    test_input_config.cpp
    held_keys.cpp
    engine_stubs.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DEFFILE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/FILES.CPP
    # FILES.CPP falls back to the disc-image source; pull in its (unexercised
    # here) implementation and the ISO9660 reader it needs so the link resolves.
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/DISCIMG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ISO9660.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOADSAVE.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOGPRINT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LOG.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/ITOA.CPP)

foreach(target test_input_bindings test_input_config)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/LIB386/H
        ${CMAKE_SOURCE_DIR}/SOURCES
    )

    # KEYBOARD_KEYS.H pulls in <SDL3/SDL_keycode.h>; link SDL3 for its include
    # dirs. Matches tests/input_funnel.
    target_link_libraries(${target} PRIVATE SDL3::SDL3)

    # LogPrintf routes through the structured log core; bypass the SDL_Log spine
    # so the log calls in the code under test need no SDL runtime.
    target_compile_definitions(${target} PRIVATE LBA_LOG_NO_SDL)

    # Both tests use variadic macros and vsnprintf, which are C++11; the project
    # default is C++98. Pinned as the other host tests that need it are.
    set_target_properties(${target} PROPERTIES CXX_STANDARD 11)

    add_test(NAME ${target} COMMAND ${target})
    register_host_test(${target})
endforeach()
