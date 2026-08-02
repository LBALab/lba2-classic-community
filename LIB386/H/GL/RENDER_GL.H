
#pragma once
#include <POLYGON/POLY.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
//! Initialize the GL renderer. Creates shaders, textures, and GL state.
//! Must be called after SDL window and GL context creation.
//! Returns TRUE on success, FALSE if GL initialization fails.
S32 GL_Renderer_Init(U32 resX, U32 resY);

//! Shut down the GL renderer and release GL resources.
void GL_Renderer_Shutdown(void);

//! Notify the GL renderer of a resolution change (recreate viewport/textures).
void GL_Renderer_Resize(U32 resX, U32 resY);

//! Begin a new frame: clear the depth buffer so that GL_DEPTH_TEST starts
//! with a clean slate. Must be called once per frame before any 3D drawing.
void GL_BeginFrame(void);

//! Clear the FBO color and depth buffers at the start of terrain rendering.
//! Called from AffGrilleExt() so that frames with no terrain (e.g. standing
//! still) preserve the previous frame's 3D content in the FBO.
void GL_ClearFBO(void);

//! Present a palette-converted ARGB framebuffer via GL with optional
//! bilinear filtering. Called from PresentFrame() when GPURendererActive.
//! @param argbPixels  ARGB8888 pixel data (ModeDesiredX * ModeDesiredY)
//! @param pitch       Row pitch in bytes
//! @param width       Framebuffer width
//! @param height      Framebuffer height
//! @param filterLinear  true = GL_LINEAR (bilinear), false = GL_NEAREST
void GL_Present(const void *argbPixels, U32 pitch, U32 width, U32 height,
                S32 filterLinear);

//! Render a list of projected triangles via OpenGL.
//! Called from Fill_Poly() when GPURendererActive is true.
S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints);

//! Set the internal render quality multiplier (1 = native, 2 = SSAA 2x, 4 = SSAA 4x).
//! Recreates the FBO at the new internal resolution.  Safe to call at any time;
//! takes effect on the next frame.
void GL_SetRenderQuality(U32 quality);

//! Flush any pending batch of triangles to the FBO.
//! Must be called after mid-frame rendering (e.g. DrawAnimatedPolys) that
//! adds geometry to the batch outside the AffGrilleExt terrain pass, so that
//! the triangles actually reach the FBO before the next readback or present.
//! No-op when the batch is empty.
void GL_FlushBatch(void);

//! Read back the FBO depth buffer into the SW Z-buffer (PtrZBuffer) after
//! terrain rendering.  Converts GL_DEPTH_COMPONENT16 back to the aZ format
//! used by ZBufBoxOverWrite2() for shadow/object occlusion in GL mode.
//! Handles SSAA downscaling (FBO may be larger than PtrZBuffer).
//! No-op when no FBO is active (s_fbo == 0).
void GL_ReadbackDepthBuffer(void);

//! Read back the FBO color buffer and convert ARGB pixels to 8-bit palette
//! indices, storing the result in the Screen buffer.  Used by AffGrilleExt()
//! to copy terrain into Log so ShadeBoxBlk (shadow darkening) has real pixels
//! to operate on.  Must be called after GL_ReadbackDepthBuffer() (FBO must
//! be bound).  Handles SSAA downscaling.  No-op when no FBO is active.
void GL_ReadbackColorBuffer(void);

// -----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif
