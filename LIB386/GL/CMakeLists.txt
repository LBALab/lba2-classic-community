add_library(gly RENDER_GL.CPP TRIANGULATE.CPP NEARCLIP.CPP)

target_include_directories(gly PUBLIC ${CMAKE_SOURCE_DIR}/LIB386/H)
target_compile_definitions(gly PRIVATE USE_GPURENDERER)
target_link_libraries(gly PUBLIC sys)

if(ANDROID)
    # GLES 2.0 symbols live in libGLESv2.so in the NDK sysroot; the CMake
    # OpenGL package does not expose it, so link it directly. dlopen is
    # not an option here because the renderer references these symbols
    # statically at link time.
    if(ANDROID)
        # GLES 3 symbols (blitFramebuffer, mapBufferRange) live in libGLESv3.so.
        target_link_libraries(gly PUBLIC GLESv3)
    else()
        target_link_libraries(gly PUBLIC GLESv2)
    endif()
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(gly PUBLIC OpenGL::GL)
endif()
