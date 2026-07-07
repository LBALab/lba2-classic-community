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

# Layer 4b: writer-side round-trip. INTENTIONALLY RED - SAVEGAME_WIRE.CPP does
# not define the ...ToWire32 writers yet, so this target fails to LINK. It is
# EXCLUDE_FROM_ALL so `host_tests` / CI keep building the green suite; build it
# explicitly to see the red:
#     cmake --build <dir> --target test_save_wire_to32   # link error until Phase 2
# Phase 2 implements the writers, then this drops EXCLUDE_FROM_ALL and gains
# add_test + register_host_test.
add_executable(test_save_wire_to32 EXCLUDE_FROM_ALL test_save_wire_to32.cpp
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
