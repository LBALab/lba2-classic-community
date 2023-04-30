#include <3D/SQRROOT.H>

#include <math.h>

U32 Sqr(U32 x)
{
  // This one has been tested against the original implementation and the results
  // match 100% for all values of U32.
  return sqrt(x);
}

U32 QSqr(U32 xLow, U32 xHigh)
{
  // TODO: revisit this. Implement same as original game to ensure same results
  U64 x = xLow | ((U64)xHigh << 32);
  return sqrtl(x);
}
