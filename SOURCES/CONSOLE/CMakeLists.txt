add_library(console CONSOLE.CPP)
target_include_directories(console PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_SOURCE_DIR}/LIB386/H
)
target_compile_definitions(console PRIVATE CONSOLE_MODULE ONE_GAME_DIRECTORY LBA_GAME CDROM SDL_DISABLE_OLD_NAMES)
