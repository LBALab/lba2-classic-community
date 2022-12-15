#include <3D/DISTANCE.H>

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

U32 Distance2D(S32 x0, S32 y0, S32 x1, S32 y1) {
  S32 dx = x0 - x1;
  S32 dy = y0 - y1;

  return sqrtl(dx * dx + dy * dy);
}

U32 Distance3D(S32 x0, S32 y0, S32 z0, S32 x1, S32 y1, S32 z1) {
  S32 dx = x0 - x1;
  S32 dy = y0 - y1;
  S32 dz = z0 - z1;

  return sqrtl(dx * dx + dy * dy + dz * dz);
}

#ifdef __cplusplus
}
#endif
