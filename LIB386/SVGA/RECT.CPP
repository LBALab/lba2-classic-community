#include <SVGA/RECT.H>

#include <SVGA/FIL_LINE.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
void Rect(S32 x0, S32 y0, S32 x1, S32 y1, S32 coul) {
  Line(x0, y0, x1, y0, coul);
  Line(x0, y1, x1, y1, coul);
  Line(x0, y0, x0, y1, coul);
  Line(x1, y0, x1, y1, coul);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
