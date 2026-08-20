#pragma once

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define ade_max(a, b)           \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _a : _b;      \
    })

#define ade_min(a, b)           \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })

// -----------------------------------------------------------------------------
// FIXME: Find places that depend on this pack(1) and do the correct push/pop
#pragma pack(push, 1)

#include "ADELINE_TYPES.H"

#pragma pack(pop)

#ifdef _WIN32
#define ADELINE_PATH_SEP "\\"
#define ADELINE_PATH_SEP_CHAR '\\'
#else // _WIN32
#define ADELINE_PATH_SEP "/"
#define ADELINE_PATH_SEP_CHAR '/'
#endif // _WIN32

// =============================================================================
#ifdef __cplusplus
}
#endif
