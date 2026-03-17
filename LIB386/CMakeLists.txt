include_directories(H)

# Enable debug traces for ASM-vs-CPP comparison when building tests
if(LBA2_BUILD_TESTS)
  add_definitions(-DENABLE_LIB386_DEBUG_TRACES)
endif()

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
