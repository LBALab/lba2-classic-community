# Small host utilities (no SDL). Optional: set OFF for minimal configures if needed.
option(LBA2_BUILD_SAVE_TOOLS "Build save inspection helpers (save_decompress)" ON)

if(LBA2_BUILD_SAVE_TOOLS)
    add_executable(save_decompress save_decompress.cpp ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/LZ.CPP)
    target_include_directories(save_decompress PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    # Match LIB386 C++ dialect
    set_target_properties(save_decompress PROPERTIES CXX_STANDARD 98)
endif()

# What the 3D maths costs per platform, float path against integer path.
# scripts/dev/bench-3d-path.sh turns this on and runs it here or on a phone.
option(LBA2_BUILD_BENCH "Build the 3D maths bench (bench_3d_path)" OFF)

if(LBA2_BUILD_BENCH)
    add_executable(bench_3d_path bench_3d_path.cpp)
    target_include_directories(bench_3d_path PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(bench_3d_path PRIVATE 3D svg sys)
endif()
