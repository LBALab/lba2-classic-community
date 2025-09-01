#pragma once

#include <SVGA/MASK.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
extern void *PtrFont;
extern S32 InterLeave;
extern S32 InterSpace;

// -----------------------------------------------------------------------------
extern S32 SizeFont(char *str);
extern S32 CarFont(S32 x, S32 y, U8 c);
extern S32 Font(S32 x, S32 y, const char *str);

// -----------------------------------------------------------------------------
#define ColorFont(color) ColorMask(color)

#define SetFont(ptrfont, interleave, interspace)                               \
  {                                                                            \
    PtrFont = (ptrfont);                                                       \
    InterLeave = (interleave);                                                 \
    InterSpace = (interspace);                                                 \
  }

// =============================================================================
#ifdef __cplusplus
}
#endif
