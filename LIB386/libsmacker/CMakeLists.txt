add_library(libsmacker STATIC
  smacker.c
  smk_bitstream.c
  smk_hufftree.c
  smacker_compat.c
)
target_include_directories(libsmacker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
