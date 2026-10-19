# Host test for the header-only half of the settings layer (SOURCES/SETTINGS.H): the coercion
# rules, where collapsing them changed what six settings boot into, and the recording header's
# settings lines. The rest of the layer talks to the cfg reader and the console.
add_executable(test_settings_coerce test_settings_coerce.cpp)

target_include_directories(test_settings_coerce PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES  # SETTINGS.H
    ${CMAKE_SOURCE_DIR}/LIB386/H # SYSTEM/ADELINE_TYPES.H (S32)
    ${CMAKE_SOURCE_DIR}/tests    # test_harness.h
)
set_target_properties(test_settings_coerce PROPERTIES CXX_STANDARD 11)

add_test(NAME test_settings_coerce COMMAND test_settings_coerce)
register_host_test(test_settings_coerce)

# A replay's borrow of the recording's settings, through a real cfg file: the borrowed value
# must never be what the cfg writer persists. Links SETTINGS.CPP and the cfg reader; the
# console it registers cvars with is stubbed in the test.
add_executable(test_settings_borrow
    test_settings_borrow.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/SETTINGS.CPP
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

target_include_directories(test_settings_borrow PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)
target_compile_definitions(test_settings_borrow PRIVATE LBA_LOG_NO_PLATFORM)
set_target_properties(test_settings_borrow PROPERTIES CXX_STANDARD 11)

add_test(NAME test_settings_borrow COMMAND test_settings_borrow)
register_host_test(test_settings_borrow)

# The retail-fix rows' arithmetic (SOURCES/RETAILFIX.H): the master row over each fix's
# own, and the options menu's cycle. Header-only, like the coercion above.
add_executable(test_retail_fix test_retail_fix.cpp)

target_include_directories(test_retail_fix PRIVATE
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)
set_target_properties(test_retail_fix PROPERTIES CXX_STANDARD 11)

add_test(NAME test_retail_fix COMMAND test_retail_fix)
register_host_test(test_retail_fix)
