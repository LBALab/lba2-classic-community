
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

//! Present an 8-bit palette-index framebuffer by expanding it through a
//! GPU palette texture, skipping the CPU palette pass entirely.  Index 0
//! is transparent (the palette's entry 0 must carry alpha 0, as forced by
//! PresentFrame).  Otherwise identical to GL_Present.
//! @param indices      8-bit palette indices (ModeDesiredX * ModeDesiredY)
//! @param pitch        Row pitch in bytes (pixels for 8-bit data)
//! @param width        Framebuffer width
//! @param height       Framebuffer height
//! @param paletteU32   256-entry 0xAARRGGBB palette (little-endian words)
//! @param filterLinear  true = GL_LINEAR (bilinear), false = GL_NEAREST
void GL_PresentIndexed(const U8 *indices, U32 pitch, U32 width, U32 height,
                       const U32 *paletteU32, S32 filterLinear);

//! Triangulate the screen-space polygon `pts` (n points) into index triples
//! written to `out` (capacity 3*(n-2)), using interior diagonals only (ear
//! clipping).  A naive fan from vertex 0 paints outside the polygon outline
//! whenever the reflex vertex lands on the apex; the SW scanline fillers
//! never do.  Pure geometry, no GL state.  Returns the number of indices
//! written.
S32 GL_TriangulatePolygon(const Struc_Point *pts, S32 n, U16 *out);

//! Render a list of projected triangles via OpenGL.
//! Called from Fill_Poly() when GPURendererActive is true.
S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints);

//! Set the internal render quality multiplier (1 = native, 2 = SSAA 2x, 4 = SSAA 4x).
//! Recreates the FBO at the new internal resolution.  Safe to call at any time;
//! takes effect on the next frame.
void GL_SetRenderQuality(U32 quality);
void GL_SetAntiAlias(S32 mode); /* 0=off, 1=FXAA, 2=MSAAx2, 3=MSAAx4, 4=SMAA */

//! Flush any pending batch of triangles to the FBO.
//! Must be called after mid-frame rendering (e.g. DrawAnimatedPolys) that
//! adds geometry to the batch outside the AffGrilleExt terrain pass, so that
//! the triangles actually reach the FBO before the next readback or present.
//! No-op when the batch is empty.
void GL_FlushBatch(void);

//! Object-pass window for the texture-array path (bodies/decors only).
//! Terrain polys keep the legacy 2D path: their snapshot/pool flush flow
//! predates the array's bind-and-upload step.
void GL_ArrayWindowBegin(void);
void GL_ArrayWindowEnd(void);

//! Copy the current FBO color (terrain + decor + sea, before bodies and
//! shadows) into an internal snapshot texture.  Call once per full redraw,
//! after the terrain pass has baked everything and before bodies draw.
void GL_SnapshotTerrain(void);

//! Blit the last GL_SnapshotTerrain() snapshot back over the FBO color.
//! Call before the object pass on incremental (AFF_OBJETS_*) frames so last
//! frame's body/shadow pixels are erased.  No-op when no snapshot matches
//! the current FBO generation (full redraws cleared it anyway).
void GL_RestoreTerrainSnapshot(void);

//! Stereo horizon share: capture the FBO after the current cube's horizon
//! (DrawHorizon2ZBuf) in eye 0, then restore it in eye 1 instead of
//! re-running the pass — the backdrop's stereo disparity is sub-pixel.  The
//! capture is bounded to the horizon's tracked footprint plus a small margin;
//! a missing-cube placeholder captures the full surface.  When @p
//! backdropStable is true and the camera pose is unchanged since the previous
//! capture, the previous capture is reused as-is.  The capture clears at
//! GL_BeginFrame, so frames that never captured (menus, mono) draw the
//! horizon normally.
void GL_CaptureHorizon(S32 backdropStable);
void GL_RestoreHorizon(void);
S32 GL_HorizonValid(void);

//! Latest horizon-capture split, for the GrillePerf diagnostic: @p resolveUs
//! and @p blitUs are the resolve and blit cost in microseconds, @p rectPct is
//! the captured region in the FBO (percent of the surface).  Both MSAA modes
//! resolve the captured rect inside the blit, so @p resolveUs is zero and
//! @p blitUs includes it.  All three are zero when the last capture was a
//! prefix-pose cache hit.
void GL_GetHorizonPerf(U32 *resolveUs, U32 *blitUs, U32 *rectPct);

//! Draw the projected ground-shadow polygon directly into the FBO (GL
//! replacement for the SW DrawShadow/ShadeBoxBlk/DrawRecover pass).  xs/ys
//! walk the Bezier-rounded outline the SW span filler fills (BuildOmbreOutline),
//! az carries the per-vertex camera depth from the same midpoint arithmetic.
//! Multiplies the scene color by (15 - level) / 15, depth-tested GEQUAL
//! against the terrain so nearer geometry clips the shadow.
void GL_DrawShadowPoly(const S16 *xs, const S16 *ys, const U16 *az, S32 n,
                       U32 level);

//! Draw one body sphere (eye, button, nostril — LBA bodies model round
//! details as shaded discs) as a depth-tested flat-color disc in the FBO.
//! GL replacement for the SW Fill_Sphere Log path, whose pixels composite
//! over the FBO without occlusion.  colorIndex is the final palette index
//! (fog conversion already applied by the caller); zo is the sphere centre
//! depth in aZ units; zWrite mirrors the SW ZBuf/NZW filler split;
//! transparent renders at 50% like Sphere_Transp.
void GL_DrawSphere(S32 cx, S32 cy, S32 radius, S32 colorIndex, U32 zo,
                   S32 zWrite, S32 transparent);
/* Whether sphere discs route to the FBO (matches body-poly acceptance). */
S32 GL_SphereTargetsFBO(void);
/* Depth-tested FBO draw for body-model line entities (necklace chains,
   whiskers): the endpoints carry their own z-buffer depths so the GPU
   interpolates the line's depth along its length.  Targets the FBO under
   the same conditions as GL_SphereTargetsFBO — check that before calling. */
void GL_DrawLineFBO(S32 x0, S32 y0, S32 x1, S32 y1, S32 colorIndex,
                    U32 zo0, U32 zo1);

//! Darken a screen-space rect by the shade level (SW ShadeBoxBlk
//! replacement — dialog bands, control panels, credits).  The SW path
//! darkens the Log pixels in place, which in GL hold no scene; this
//! multiplies the FBO rect's colors by (15 - level) / 15 instead.
//! Depth-unconditional: the band shades everything composited behind it.
void GL_ShadeBox(S32 x0, S32 y0, S32 x1, S32 y1, S32 level);

//! Read back the FBO depth buffer into the SW Z-buffer (PtrZBuffer) after
//! terrain rendering.  Converts GL_DEPTH_COMPONENT16 back to the aZ format
//! used by ZBufBoxOverWrite2() for shadow/object occlusion in GL mode.
//! Handles SSAA downscaling (FBO may be larger than PtrZBuffer).
//! No-op when no FBO is active (s_fbo == 0).
void GL_ReadbackDepthBuffer(void);

//! Read back the FBO color buffer and convert ARGB pixels to 8-bit palette
//! indices, storing the result in the Screen buffer.  Used by AffGrilleExt()
//! to copy terrain into Log so ShadeBoxBlk (shadow darkening) has real pixels
//! to operate on.  Must be called after GL_ReadbackDepthBuffer().  Handles
//! SSAA downscaling.  No-op when no FBO is active.
void GL_ReadbackColorBuffer(void);

//! Draw one gameplay sprite (eyes, hands, extras) as a chroma-keyed,
//! depth-tested quad straight into the FBO.  (x0,y0)-(x1,y1) is the
//! on-screen rect, (u0,v0)-(u1,v1) the sub-rect of the w×h sprite image,
//! az the anchor depth in aZ units.  When transparent, blends at 50%;
//! otherwise texel 0 is discarded (colour 0 = transparent).
void GL_DrawSpriteQuad(S32 x0, S32 y0, S32 x1, S32 y1,
                       S32 u0, S32 v0, S32 u1, S32 v1,
                       const void *pixels, S32 w, S32 h,
                       U16 az, S32 transparent);

//! Composite the current 3D FBO color with the Log overlay (index 0
//! transparent) exactly as GL_Present draws them, and write the result as
//! an RGB PNG. The Log buffer alone holds only the 2D overlay in GL mode,
//! so screenshot tools must go through this to capture the rendered frame.
//! No-op when the GL renderer is not initialized.
void GL_SavePresentPNG(const char *path);

//! Get the present FBO handle (for OpenXR present on Quest).
//! Returns the FBO that GL_Present composites into, or 0 if not initialized.
U32 GL_GetPresentFBO(void);

//! Get the present FBO color texture handle (for OpenXR present on Quest).
//! Returns the texture attached to the present FBO, or 0 if not initialized.
U32 GL_GetPresentColorTex(void);
/* Quest present fast path: reports whether the last indexed present is
   safe to read as the scene alone, skipping the intermediate composite
   (empty overlay, no post-process, content-sized FBO). */
S32 GL_PresentSceneDirect(void);
//! Scene color texture and single-sample read FBO.  Resolve-in-tile exposes
//! the dedicated resolve texture/view; explicit MSAA returns the regular
//! s_fbo color texture/view.
U32 GL_Get3DColorTex(void);
U32 GL_Get3DFBO(void);

//! Stereo (dual-eye) capture control. With eye >= 0, the next GL_Present
//! draws the HUD into that eye's half of the scene target and skips the
//! window swap until the second eye is ready. Also sets the 3D
//! camera/projection eye shifts (see StereoCamShiftX / StereoProjShiftX in
//! 3D/CAMERA.H); eye -1 restores mono and releases the double-wide target
//! when stereo is no longer active.
void GL_SetStereoEye(S32 eye);

//! Current stereo capture eye (-1 = mono / not capturing).
S32 GL_GetStereoEye(void);

//! True when eye 1 can keep eye 0's Log overlay: the pair is active and
//! eye 0 drew only static HUD content.
S32 GL_StereoOverlayReusable(void);

//! Monotonic token for the current stereo pair, incremented when eye 0
//! starts.  Per-pair caches use it to reject data left by an earlier frame.
U32 GL_GetStereoFrameSeq(void);

//! Returns 1 if a fresh left+right eye capture pair is available for this
//! frame and consumes the flag. PresentMono uses it to decide between the
//! stereo layer and the mono full-image copy.
S32 GL_TakeStereoPair(void);

//! Offscreen FBO holding the captured composite for the given eye (0 = left,
//! 1 = right), or 0 if none. For OpenXR_PresentMono's stereo blit.
U32 GL_GetStereoEyeFBO(S32 eye);

//! Single-sample FBO containing both eye halves side by side, or 0 when the
//! double-wide stereo path is inactive.
U32 GL_GetStereoPairFBO(void);

//! Width and height of the double-wide stereo source, or 0 when inactive.
U32 GL_GetStereoPairWidth(void);
U32 GL_GetStereoPairHeight(void);

//! Returns 1 when the double-wide pair needs the display-transform shader
//! instead of a plain blit into the swapchain.
S32 GL_StereoPairNeedsPresentShader(void);

//! 1 when the double-wide MSAA target can render directly into a runtime
//! swapchain texture (resolve-in-tile extension present, neutral display
//! transform, MSAA active).  The caller must still call
//! GL_BeginDirectStereo with the acquired texture and size.
S32 GL_DirectStereoSupported(void);

//! Attach an acquired runtime swapchain texture to a fresh multisample scene
//! FBO and make it the draw target.  Returns 0 and leaves the normal scene
//! target active on any mismatch or incompleteness.  The caller must call
//! GL_EndDirectStereo before releasing the swapchain image.
S32 GL_BeginDirectStereo(U32 texture, U32 width, U32 height);

//! Detach and delete the direct scene FBO.  Safe to call when inactive.
void GL_EndDirectStereo(void);

//! 1 while the direct scene FBO is the active draw target.
S32 GL_DirectStereoActive(void);

//! Draw the resolved double-wide pair into the currently bound framebuffer,
//! applying the display transform. The target FBO is bound by the caller;
//! this sets the viewport to cover dstWidth x dstHeight. Returns 1 if the
//! draw was issued.
S32 GL_BlitStereoPairToCurrent(U32 srcWidth, U32 srcHeight, U32 dstWidth,
                               U32 dstHeight);

//! Returns 1 when eye 1's composite was skipped because the overlay was
//! empty; the stereo present then reads GL_Get3DFBO() for the right eye.
S32 GL_StereoEyeSceneDirect(void);

//! One vertex of the static terrain path: cube-local world position, the
//! batch's 8.8 fixed-point atlas UV convention, the per-vertex light, the
//! packed material word, and the tile's top-left corner (the whole tile
//! takes its fog row from that corner).  Cube-local positions keep the
//! rotated camera and the vertices in the same small frame the CPU path
//! uses after SetPosCamera(), so float precision is not spent on the
//! absolute world position of a 524k-unit map.
typedef struct {
    float X, Y, Z;
    float U, V;
    float Light;
    float RepMask;
    float Material;
    float FogX, FogY, FogZ;
} T_GL_TerrainVertex;

//! 1 when the terrain program can render static exterior terrain this frame:
//! a stereo eye is active (or LBA2_GPU_TERRAIN=1 forces it for desktop
//! tests), the renderer and terrain program are up, and LBA2_GPU_TERRAIN != 0.
S32 GL_TerrainGPUActive(void);

//! Counter that changes whenever GL objects from an earlier context or
//! program generation become invalid.  Cached VBO owners compare it and
//! drop stale handles without deleting them.
U32 GL_TerrainVBOEpoch(void);

//! Upload a static terrain VBO.  Returns 0 on failure.
U32 GL_CreateTerrainVBO(const T_GL_TerrainVertex *verts, U32 vertexCount);

//! Delete a terrain VBO.  A stale handle (renderer shut down) is ignored.
void GL_DeleteTerrainVBO(U32 vbo);

//! Draw a terrain VBO into the current scene target (correct eye viewport,
//! scene FBO, depth test/write, back-face culling).  Flushes any pending
//! batch so decors drawn just before it keep their order.  startZFog,
//! clipZFar and palLevel are the current cube's fog parameters, exactly as
//! the CPU tile loop reads them.
void GL_DrawTerrainVBO(U32 vbo, U32 vertexCount, const void *atlasPage,
                       S32 startZFog, S32 clipZFar, S32 palLevel);

// -----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif
