#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
extern S32 ArgC;
extern char *ArgV[];

// -----------------------------------------------------------------------------
void RemoveParam(S32 param);
S32 FindAndRemoveParam(const char *str);
void GetCmdLine(S32 argc, char *argv[]);

// =============================================================================
#ifdef __cplusplus
}
#endif
