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
  // After testing this against the original implementation, it seems that the
  // original implementation is not correct. The results are not the same for
  // all values of U64. The results are the same for all values of U32 though.
  // The first discordance I noticed was for:
  // QSqr(1152941149870941879) = 1073750970
  // sqrtl(1152941149870941879) = 1073750971
  // So far every discordance I've seen is a +1 for sqrtl, so it might not be a
  // big deal.
  // TODO: copy original implementation?
  U64 x = xLow | ((U64)xHigh << 32);
  return sqrtl(x);
}
