add_library(gly RENDER_GL.CPP TRIANGULATE.CPP NEARCLIP.CPP)

target_include_directories(gly PUBLIC ${CMAKE_SOURCE_DIR}/LIB386/H)
target_link_libraries(gly PUBLIC sys)

# The library is always built: with GPURENDERER off RENDER_GL.CPP compiles
# its no-op stubs, so renderer-agnostic callers still link without OpenGL.
if(GPURENDERER)
    target_compile_definitions(gly PRIVATE USE_GPURENDERER)

    if(ANDROID)
        # GLES 3 symbols (blitFramebuffer, mapBufferRange) live in libGLESv3.so.
        target_link_libraries(gly PUBLIC GLESv3)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(gly PUBLIC OpenGL::GL)
    endif()
endif()
