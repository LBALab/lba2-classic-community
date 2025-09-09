
#include <SVGA/SCALESPT.H>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  U8 Delta_X;
  U8 Delta_Y;
  U8 Hot_X;
  U8 Hot_Y;
} Struc_Sprite;

void ScaleSprite(S32 num, S32 x, S32 y, S32 factorx, S32 factory, void *ptrbank)
{
  U32 *tab = (U32 *)ptrbank;
  Struc_Sprite *sprite = (Struc_Sprite *)((U8 *)tab + tab[num]);
  S32 sx = sprite->Hot_X + x;
  S32 sy = sprite->Hot_Y + y;
  S32 sw = sprite->Delta_X;
  S32 sh = sprite->Delta_Y;

  // Clipping
  S32 start_x = 0, start_y = 0;
  S32 end_x = sx + sw;
  S32 end_y = sy + sh;
  if (sx < ClipXMin) {
    start_x = ClipXMin - sx;
    sx = ClipXMin;
  }
  if (sy < ClipYMin) {
    start_y = ClipYMin - sy;
    sy = ClipYMin;
  }
  if (end_x > ClipXMax + 1) {
    end_x = ClipXMax + 1;
  }
  if (end_y > ClipYMax + 1) {
    end_y = ClipYMax + 1;
  }
  if (sx >= end_x || sy >= end_y) {
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
    return;
  }
  ScreenXMin = sx;
  ScreenXMax = end_x;
  ScreenYMin = sy;
  ScreenYMax = end_y;

  U8 *src = (U8 *)sprite + sizeof(Struc_Sprite) + start_y * sw + start_x;
  U8 *dst = (U8 *)Log + TabOffLine[sy] + sx;
  S32 dst_line_offset = ModeDesiredX - (end_x - sx);
  S32 src_line_offset = sw - (end_x - sx);
  for (S32 y = sy; y < end_y; ++y) {
    for (S32 x = sx; x < end_x; ++x) {
      if (*src) {
        *dst = *src;
      }
      ++src;
      ++dst;
    }
    src += src_line_offset;
    dst += dst_line_offset;
  }
}

#ifdef __cplusplus
}
#endif
