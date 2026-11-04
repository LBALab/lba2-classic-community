#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/*! Whether the double-wide single-pass stereo target can carry the pair.
 *
 *  The target renders both eyes into one wide scene buffer and lets the
 *  shader apply the eye-1 delta, which is the only way the isometric interior
 *  gets parallax at all: its projection is affine and ignores the per-eye
 *  camera shift, so the per-eye fallback renders both halves from the same
 *  camera.  FXAA (antiAlias 1) runs as a per-eye composite post pass with no
 *  double-wide equivalent, so it is the one mode that keeps the fallback;
 *  MSAA and AA off both render the pair directly.
 *
 *  @param stereo3D     DisplayStereo3D (the display option).
 *  @param openxrActive the OpenXR session owns the view.
 *  @param stereoEnabled OpenXR stereo submission is on.
 *  @param antiAlias    0 = off, 1 = FXAA, 2 = MSAA x2, 3 = MSAA x4.
 */
static inline S32 GL_StereoDoubleWideAllowedFor(S32 stereo3D, S32 openxrActive,
                                                S32 stereoEnabled,
                                                S32 antiAlias) {
    return stereo3D && openxrActive && stereoEnabled && antiAlias != 1;
}

// -----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif
