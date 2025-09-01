#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/S_MALLOC.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define MALLOC_ALIGN 32

// -----------------------------------------------------------------------------
void *NormMalloc(U32 size);
void NormFree(void *ptr);
void *NormMshrink(void *ptr, U32 size);

// -----------------------------------------------------------------------------
#define Malloc NormMalloc

#ifdef DEBUG_MALLOC
#define Mshrink(a, b) SafeMshrink(a, b, __FILE__, __LINE__)
#define Free(a) SafeFree(a, __FILE__, __LINE__)
#else // DEBUG_MALLOC
#define Mshrink(a, b) NormMshrink(a, b)
#define Free(a) NormFree(a)
#endif // DEBUG_MALLOC

#define SmartMalloc(size) Malloc(size)

// =============================================================================
#ifdef __cplusplus
}
#endif
