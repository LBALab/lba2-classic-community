#include <SYSTEM/ITOA.H>

#include <stdio.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
char *Itoa(int value) {
  static char ItoaTempoString[33];
  snprintf(ItoaTempoString, 33, "%d", value);
  return ItoaTempoString;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
