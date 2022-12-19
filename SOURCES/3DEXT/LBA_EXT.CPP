#include "LBA_EXT.H"

#define max(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })

#define min(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a < _b ? _a : _b;                                                         \
  })

S32 Min3(S32 a, S32 b, S32 c) {
  S32 tmp = min(a, b);
  return min(tmp, c);
}

S32 Max3(S32 a, S32 b, S32 c) {
  S32 tmp = max(a, b);
  return max(tmp, c);
}
