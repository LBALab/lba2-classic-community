# Save wire-format host tests (docs/SAVE_WIRE_PLAN.md, Phase 1).
# Bit-exact retail save serialization: the ...ToWire32 / ...FromWire32 converters.
# No retail data - golden byte-vectors only, runs in public CI.

# Layer 4a: independent layout anchor + ...FromWire32 golden decode. GREEN.
add_executable(test_save_wire_from32 test_save_wire_from32.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/SAVEGAME_WIRE.CPP)
target_include_directories(test_save_wire_from32 PRIVATE
    # LIB386/H before SOURCES so <OBJECT.H> in the DEFINES.H chain resolves to
    # the lib header (matching the engine), not SOURCES/OBJECT.H which needs
    # T_OBJET defined first. SOURCES stays on the path for quoted includes.
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/tests
)
set_target_properties(test_save_wire_from32 PROPERTIES CXX_STANDARD 11)
add_test(NAME test_save_wire_from32 COMMAND test_save_wire_from32)
register_host_test(test_save_wire_from32)

# Layer 4b: writer-side round-trip through the ...ToWire32 writers (Phase 2 GREEN).
add_executable(test_save_wire_to32 test_save_wire_to32.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/SAVEGAME_WIRE.CPP)
target_include_directories(test_save_wire_to32 PRIVATE
    # LIB386/H before SOURCES so <OBJECT.H> in the DEFINES.H chain resolves to
    # the lib header (matching the engine), not SOURCES/OBJECT.H which needs
    # T_OBJET defined first. SOURCES stays on the path for quoted includes.
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/tests
)
set_target_properties(test_save_wire_to32 PROPERTIES CXX_STANDARD 11)
add_test(NAME test_save_wire_to32 COMMAND test_save_wire_to32)
register_host_test(test_save_wire_to32)

# Fuzz: randomized write->read->write idempotency over the three converters.
add_executable(test_save_wire_fuzz test_save_wire_fuzz.cpp
    ${CMAKE_SOURCE_DIR}/SOURCES/SAVEGAME_WIRE.CPP)
target_include_directories(test_save_wire_fuzz PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/SOURCES
    ${CMAKE_SOURCE_DIR}/tests
)
set_target_properties(test_save_wire_fuzz PROPERTIES CXX_STANDARD 11)
add_test(NAME test_save_wire_fuzz COMMAND test_save_wire_fuzz)
register_host_test(test_save_wire_fuzz)
