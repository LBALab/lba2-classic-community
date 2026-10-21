# The recording header: field lookup and the two binding tables it carries. Pure
# string and table work, so the link is the module under test plus INPUT_BINDINGS for
# the retail tables the round trip is checked against. No engine, no SDL runtime, no
# retail data: that is what the extraction out of RECORD.CPP bought.
add_executable(test_record_format
    test_record_format.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/RECORD_FORMAT.CPP
    ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP
    # INPUT.CPP's two keyboard hooks, stubbed. Shared with tests/input_bindings so
    # there is one stub rather than a second that can drift from it.
    ${CMAKE_SOURCE_DIR}/tests/input_bindings/held_keys.cpp)

target_include_directories(test_record_format PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests/input_bindings
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
)

# The format and binding tables need no platform diagnostic transport.
target_compile_definitions(test_record_format PRIVATE LBA_LOG_NO_PLATFORM)

set_target_properties(test_record_format PROPERTIES CXX_STANDARD 11)

add_test(NAME test_record_format COMMAND test_record_format)
register_host_test(test_record_format)

foreach(target test_record_binding_context test_record_binding_context_stick_layout test_record_binding_context_alternate)
    add_executable(${target}
        test_record_binding_context.cpp
        ${CMAKE_SOURCE_DIR}/SOURCES/RECORD_FORMAT.CPP
        ${CMAKE_SOURCE_DIR}/SOURCES/INPUT_BINDINGS.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT.CPP
        ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/INPUT_FLOW.CPP
        ${CMAKE_SOURCE_DIR}/tests/input_bindings/held_keys.cpp)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/input_bindings
        ${CMAKE_SOURCE_DIR}/LIB386/H
        ${CMAKE_SOURCE_DIR}/SOURCES)
    target_compile_definitions(${target} PRIVATE LBA_LOG_NO_PLATFORM)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 11)
    add_test(NAME ${target} COMMAND ${target})
    register_host_test(${target})
endforeach()
lba2_target_input_profile(test_record_binding_context "${CMAKE_SOURCE_DIR}/cmake/input/default.cmake")
lba2_target_input_profile(test_record_binding_context_stick_layout "${CMAKE_SOURCE_DIR}/cmake/input/vita.cmake")
lba2_target_input_profile(test_record_binding_context_alternate "${CMAKE_SOURCE_DIR}/tests/input_bindings/alternate_profile.cmake")

find_program(RECORD_TEST_PYTHON NAMES python3)
if(RECORD_TEST_PYTHON)
    add_custom_target(test_record_ui_stream)
    add_test(NAME test_record_ui_stream
        COMMAND ${RECORD_TEST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/test_dump_recording.py)
    register_host_test(test_record_ui_stream)
endif()
