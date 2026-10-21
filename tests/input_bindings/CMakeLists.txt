# The binding table and the fold that builds Input from it. Driven through the
# real GetInput(), so LIB386/SYSTEM/INPUT.CPP links alongside; the two keyboard
# hooks are stubbed, which is what keeps SDL and the game stack out. Nothing
# else links: that is what the extraction bought.
add_executable(test_input_bindings
    test_input_bindings.cpp
    held_keys.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP)

# NoRepeatInput, the latch that turns a held key into one press. Same lean link
# as the table test: the rules live in LIB386/SYSTEM/INPUT.CPP and the bindings
# it masks come from the extracted module.
add_executable(test_input_suppression
    test_input_suppression.cpp
    held_keys.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP)

# The other direction: which keys carry a slot, and which slot a key carries.
# Driven through the real GetInput() as the table test is, because the claim
# worth pinning is that the two directions agree rather than that either is
# self-consistent.
add_executable(test_input_reverse
    test_input_reverse.cpp
    held_keys.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP)

# Bindings through the config file, on a temp cfg in the test's own directory.
# SOURCES/INPUT.CPP still holds the polling loop, so this one carries the stubs.
add_executable(test_input_config
    test_input_config.cpp
    held_keys.cpp
    engine_stubs.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/EMBEDDED_CFG_WRITE.CPP
    ${CMAKE_BINARY_DIR}/SOURCES/embedded_lba2_cfg_data.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/MENU_INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/TIMER.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP
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

target_link_libraries(test_input_config PRIVATE lba2_platform)

get_target_property(_INPUT_CONFIG_SOURCES test_input_config SOURCES)
add_executable(test_input_config_vita ${_INPUT_CONFIG_SOURCES})
lba2_target_input_profile(test_input_config_vita "${CMAKE_SOURCE_DIR}/cmake/input/vita.cmake")
target_link_libraries(test_input_config_vita PRIVATE lba2_platform)
add_executable(test_input_config_alternate ${_INPUT_CONFIG_SOURCES})
lba2_target_input_profile(test_input_config_alternate "${CMAKE_CURRENT_SOURCE_DIR}/alternate_profile.cmake")
target_link_libraries(test_input_config_alternate PRIVATE lba2_platform)
add_executable(test_input_config_android ${_INPUT_CONFIG_SOURCES})
lba2_target_input_profile(test_input_config_android "${CMAKE_SOURCE_DIR}/cmake/input/android.cmake")
target_link_libraries(test_input_config_android PRIVATE lba2_platform)

foreach(target test_input_config test_input_config_vita test_input_config_alternate test_input_config_android)
    target_compile_definitions(${target} PRIVATE TEST_CFG_PATH="${target}.cfg")
endforeach()

add_test(NAME test_input_profile_config
    COMMAND ${CMAKE_COMMAND}
        "-DMODULE=${CMAKE_SOURCE_DIR}/cmake/input-profile.cmake"
        "-DROOT=${CMAKE_SOURCE_DIR}"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/profile-config-tests"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_input_profile.cmake)
set_tests_properties(test_input_profile_config PROPERTIES LABELS host_quick)

set(_INPUT_CONFIG_SUPPORT ${_INPUT_CONFIG_SOURCES})
list(REMOVE_ITEM _INPUT_CONFIG_SUPPORT test_input_config.cpp)
add_executable(test_input_vita test_input_vita.cpp ${_INPUT_CONFIG_SUPPORT})
lba2_target_input_profile(test_input_vita "${CMAKE_SOURCE_DIR}/cmake/input/vita.cmake")
target_link_libraries(test_input_vita PRIVATE lba2_platform)

foreach(target test_input_config test_input_config_vita test_input_config_alternate test_input_config_android test_input_vita)
    add_dependencies(${target} lba2_embedded_cfg)
endforeach()

add_executable(test_input_context test_input_context.cpp held_keys.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP)
lba2_target_input_profile(test_input_context "${CMAKE_CURRENT_SOURCE_DIR}/alternate_profile.cmake")

foreach(target test_input_bindings test_input_suppression test_input_reverse test_input_config)
    lba2_target_input_profile(${target} "${CMAKE_SOURCE_DIR}/cmake/input/default.cmake")
endforeach()

foreach(target test_input_bindings test_input_suppression test_input_reverse test_input_config test_input_config_vita test_input_config_alternate test_input_config_android test_input_vita test_input_context)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/LIB386/H
        ${CMAKE_SOURCE_DIR}/SOURCES
    )

    # The pure-data tests need no platform diagnostic transport.
    target_compile_definitions(${target} PRIVATE LBA_LOG_NO_PLATFORM)

    # Both tests use variadic macros and vsnprintf, which are C++11; the project
    # default is C++98. Pinned as the other host tests that need it are.
    set_target_properties(${target} PROPERTIES CXX_STANDARD 11)

    add_test(NAME ${target} COMMAND ${target})
    register_host_test(${target})
endforeach()
