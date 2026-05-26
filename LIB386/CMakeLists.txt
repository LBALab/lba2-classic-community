include_directories(H)

# LIB386 debug traces are an ASM-vs-CPP equivalence check tool — every shim in
# LIB386_TRACE_CPP / LIB386_TRACE_ASM prints "[CPP][Func][N] ..." lines that the
# ASM equivalence tests diff against the matching "[ASM]" lines. The macros are
# defined inline as ((void)0) when ENABLE_LIB386_DEBUG_TRACES isn't set.
#
# This define was previously applied at directory scope (add_definitions when
# LBA2_BUILD_TESTS was on), which leaked into every target built in the LIB386
# subdirs — including the static libraries linked into the shipping engine
# binary. The engine then wrote ~22K [CPP] lines per tick to stdout on any
# build configured with -DLBA2_BUILD_TESTS=ON (e.g. the default `make build`
# after a CI-style configure). Scope of the leak: ~58% of sys time in
# projection_demo on a WSL2 setup.
#
# The fix: only the test executables that need the traces opt in, via
# target_compile_definitions(<test> PRIVATE ENABLE_LIB386_DEBUG_TRACES) inside
# the add_asm_cpp_test() helper. Engine targets stay clean.

add_subdirectory(3D)
add_subdirectory(AIL)
add_subdirectory(ANIM)
add_subdirectory(FILEIO)
add_subdirectory(OBJECT)
add_subdirectory(pol_work)
add_subdirectory(SVGA)
add_subdirectory(SYSTEM)

if(ENABLE_POLY_RECORDING OR LBA2_BUILD_TESTS)
  add_subdirectory(SNAPSHOT)
endif()

if(MVIDEO_BACKEND STREQUAL "smacker")
  add_subdirectory(libsmacker)
endif()

if(MVIDEO_BACKEND STREQUAL "smacker" OR LBA2_BUILD_TESTS)
  add_subdirectory(VIDEO_AUDIO_RESAMPLE)
endif()
