//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_ADELINE
#define LIB_SYSTEM_ADELINE

#ifdef _WIN32 // Portability - mkdir has different includes and signature
#include <direct.h> // For ADELINE_MKDIR
#else // _WIN32
#include <sys/stat.h>  // For ADELINE_MKDIR
#include <sys/types.h> // For ADELINE_MKDIR
#endif // _WIN32

//──────────────────────────────────────────────────────────────────────────

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

//──────────────────────────────────────────────────────────────────────────
#pragma pack(1)

#include "ADELINE_TYPES.H"

#ifdef _WIN32
#define ADELINE_PATH_SEPARATOR "\\"
#define ADELINE_PATH_SEPARATOR_CHAR '\\'
#else // _WIN32
#define ADELINE_PATH_SEPARATOR "/"
#define ADELINE_PATH_SEPARATOR_CHAR '/'
#endif // _WIN32

//──────────────────────────────────────────────────────────────────────────
// There is no single portable mkdir function
#ifdef _WIN32 // Portability - mkdir has different includes and signature
#define ADELINE_MKDIR(path) _mkdir(path)
#else // _WIN32
#define ADELINE_MKDIR(path) mkdir(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#endif // _WIN32

//──────────────────────────────────────────────────────────────────────────
#ifndef TYPEDEFS
#define TYPEDEFS

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
#define	AIL_vmm_lock(a, b)
#define	AIL_vmm_unlock(a, b)

//──────────────────────────────────────────────────────────────────────────
#endif//TYPEDEFS

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif//LIB_SYSTEM_ADELINE

//──────────────────────────────────────────────────────────────────────────
