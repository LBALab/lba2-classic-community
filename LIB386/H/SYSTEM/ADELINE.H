#pragma once

#ifdef _WIN32 // Portability - mkdir has different includes and signature
#include <direct.h> // For ADELINE_MKDIR
#else // _WIN32
#include <sys/stat.h>  // For ADELINE_MKDIR
#include <sys/types.h> // For ADELINE_MKDIR
#endif // _WIN32

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define ade_max(a, b)                                                          \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })

#define ade_min(a, b)                                                          \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a < _b ? _a : _b;                                                         \
  })

// -----------------------------------------------------------------------------
// FIXME: Find places that depend on this pack(1) and do the correct push/pop
#pragma pack(1)

#include "ADELINE_TYPES.H"

#ifdef _WIN32
#define ADELINE_PATH_SEP "\\"
#define ADELINE_PATH_SEP_CHAR '\\'
#else // _WIN32
#define ADELINE_PATH_SEP "/"
#define ADELINE_PATH_SEP_CHAR '/'
#endif // _WIN32

// -----------------------------------------------------------------------------
// There is no single portable mkdir function
#ifdef _WIN32 // Portability - mkdir has different includes and signature
#define ADELINE_MKDIR(path) _mkdir(path)
#else // _WIN32
#define ADELINE_MKDIR(path) mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH)
#endif // _WIN32

// =============================================================================
#ifdef __cplusplus
}
#endif
