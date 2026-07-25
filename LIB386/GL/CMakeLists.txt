add_library(gly RENDER_GL.CPP)

target_include_directories(gly PUBLIC ${CMAKE_SOURCE_DIR}/LIB386/H)
target_compile_definitions(gly PRIVATE USE_GPURENDERER)
target_link_libraries(gly PUBLIC sys)

if(ANDROID)
    # GLES 2.0 is part of the NDK — no extra link needed.
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(gly PUBLIC OpenGL::GL)
endif()
