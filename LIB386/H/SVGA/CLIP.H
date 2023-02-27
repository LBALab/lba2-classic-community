#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
extern S32 ClipXMin;
extern S32 ClipYMin;
extern S32 ClipXMax;
extern S32 ClipYMax;

extern S32 ClipWindowXMin;
extern S32 ClipWindowYMin;
extern S32 ClipWindowXMax;
extern S32 ClipWindowYMax;

extern S32 MemoClipXMin;
extern S32 MemoClipYMin;
extern S32 MemoClipXMax;
extern S32 MemoClipYMax;

extern S32 MemoClipWindowXMin;
extern S32 MemoClipWindowYMin;
extern S32 MemoClipWindowXMax;
extern S32 MemoClipWindowYMax;

// -----------------------------------------------------------------------------
void MemoClip();
void RestoreClip();
void SetClip(S32 x0, S32 y0, S32 x1, S32 y1);
void UnsetClip();

void MemoClipWindow();
void RestoreClipWindow();
void SetClipWindow(S32 x0, S32 y0, S32 x1, S32 y1);
void UnsetClipWindow();

// =============================================================================
#ifdef __cplusplus
}
#endif
