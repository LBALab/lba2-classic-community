include_directories(H)

add_subdirectory(3D)
add_subdirectory(AIL)
add_subdirectory(ANIM)
add_subdirectory(FILEIO)
add_subdirectory(OBJECT)
add_subdirectory(pol_work)
add_subdirectory(SVGA)
add_subdirectory(SYSTEM)

if(ENABLE_SNAPSHOT_TEST_CAPTURE OR ENABLE_POLY_RECORDING OR LBA2_BUILD_TESTS)
  add_subdirectory(SNAPSHOT)
endif()

if(MVIDEO_BACKEND STREQUAL "smacker")
  add_subdirectory(libsmacker)
endif()
