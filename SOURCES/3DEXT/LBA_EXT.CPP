#include "LBA_EXT.H"

#include <SYSTEM/ADELINE.H>

S32 Min3(S32 a, S32 b, S32 c) {
  S32 tmp = ade_min(a, b);
  return ade_min(tmp, c);
}

S32 Max3(S32 a, S32 b, S32 c) {
  S32 tmp = ade_max(a, b);
  return ade_max(tmp, c);
}
