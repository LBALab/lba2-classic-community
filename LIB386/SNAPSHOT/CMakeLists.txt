add_library(snapshot SNAPSHOT.CPP POLY_RECORDING.CPP)
target_include_directories(snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../H)
