#include <3D/SQRROOT.H>

#include <math.h>
#include <SYSTEM/DEBUG_TRACES.H>

U32 Sqr(U32 x)
{
  LIB386_TRACE_CPP("Sqr", "1", "x=%u", x);
  // This one has been tested against the original implementation and the results
  // match 100% for all values of U32.
  U32 result = sqrt(x);
  LIB386_TRACE_CPP("Sqr", "2", "return=%u", result);
  return result;
}

U32 QSqr(U32 xLow, U32 xHigh)
{
  LIB386_TRACE_CPP("QSqr", "1", "xLow=%u xHigh=%u", xLow, xHigh);
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
  U32 result = sqrtl(x);
  LIB386_TRACE_CPP("QSqr", "2", "return=%u", result);
  return result;
}
