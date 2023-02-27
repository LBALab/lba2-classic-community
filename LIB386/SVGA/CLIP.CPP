#include <SVGA/CLIP.H>

#include <SVGA/VIDEO.H>
#include <SYSTEM/ADELINE.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
S32 ClipXMin = 0;
S32 ClipYMin = 0;
S32 ClipXMax = 0;
S32 ClipYMax = 0;

S32 ClipWindowXMin = 0;
S32 ClipWindowYMin = 0;
S32 ClipWindowXMax = 0;
S32 ClipWindowYMax = 0;

S32 MemoClipXMin = 0;
S32 MemoClipYMin = 0;
S32 MemoClipXMax = 0;
S32 MemoClipYMax = 0;

S32 MemoClipWindowXMin = 0;
S32 MemoClipWindowYMin = 0;
S32 MemoClipWindowXMax = 0;
S32 MemoClipWindowYMax = 0;

// -----------------------------------------------------------------------------
void MemoClip() {
  MemoClipXMin = ClipXMin;
  MemoClipXMax = ClipXMax;
  MemoClipYMin = ClipYMin;
  MemoClipYMax = ClipYMax;
}

void RestoreClip() {
  SetClip(MemoClipXMin, MemoClipYMin, MemoClipXMax, MemoClipYMax);
}

void SetClip(S32 x0, S32 y0, S32 x1, S32 y1) {
  ClipXMin = ade_max(x0, ClipWindowXMin);
  ClipXMax = ade_min(x1, ClipWindowXMax);
  ClipYMin = ade_max(y0, ClipWindowYMin);
  ClipYMax = ade_min(y1, ClipWindowYMax);
}

void UnsetClip() {
  ClipXMin = ClipWindowXMin;
  ClipXMax = ClipWindowXMax;
  ClipYMin = ClipWindowYMin;
  ClipYMax = ClipWindowYMax;
}

void MemoClipWindow() {
  MemoClipWindowXMin = ClipWindowXMin;
  MemoClipWindowXMax = ClipWindowXMax;
  MemoClipWindowYMin = ClipWindowYMin;
  MemoClipWindowYMax = ClipWindowYMax;
}

void RestoreClipWindow() {
  SetClipWindow(MemoClipWindowXMin, MemoClipWindowYMin,
                MemoClipWindowXMax, MemoClipWindowYMax);
}

void SetClipWindow(S32 x0, S32 y0, S32 x1, S32 y1) {
  ClipWindowXMin = ade_max(0, x0);
  ClipWindowXMax = ade_min(x1, ModeResX - 1);
  ClipWindowYMin = ade_max(0, y0);
  ClipWindowYMax = ade_min(y1, ModeResY - 1);

  SetClip(ClipXMin, ClipYMin, ClipXMax, ClipYMax);
}

void UnsetClipWindow() {
  ClipWindowXMax = ModeResX - 1;
  ClipXMax = ClipWindowXMax;
  ClipWindowXMin = 0;
  ClipXMin = 0;

  ClipWindowYMax = ModeResY - 1;
  ClipYMax = ClipWindowYMax;
  ClipWindowYMin = 0;
  ClipYMin = 0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
