add_library(console CONSOLE.CPP CONSOLE_STATE.CPP CONSOLE_GIVE.CPP)
target_include_directories(console PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_SOURCE_DIR}/LIB386/H
)
target_link_libraries(console PRIVATE lba2_platform)
target_compile_definitions(console PRIVATE ONE_GAME_DIRECTORY LBA_GAME CDROM SDL_DISABLE_OLD_NAMES)
