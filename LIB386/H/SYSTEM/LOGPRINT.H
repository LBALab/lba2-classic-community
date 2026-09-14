//--------------------------------------------------------------------------//
#ifndef LIB_SYSTEM_LOGPRINT
#define LIB_SYSTEM_LOGPRINT

//----------------------------------------------------------------------------
#include <SYSTEM/ADELINE_TYPES.H>

#include <stdarg.h>
#include <stdio.h>

//--------------------------------------------------------------------------//
#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------
// Moves an existing log to its previous-run path, then starts an empty one.
extern void CreateLog(const char *filePath);

// The previous-run path for a log: ".prev" before the file name's extension,
// adeline.log -> adeline.prev.log. Empty when it does not fit.
extern void GetPreviousLogPath(char *outPath, U32 outSize, const char *filePath);

//----------------------------------------------------------------------------
extern void LogPrintf(const char *format, ...);

//----------------------------------------------------------------------------
extern void LogPuts(const char *string);

//--------------------------------------------------------------------------//
#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------//
#endif //LIB_SYSTEM_LOGPRINT

//----------------------------------------------------------------------------
