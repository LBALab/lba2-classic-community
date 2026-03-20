#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#include <stddef.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
char *ade_strcasestr(const char *haystack, const char *needle);
U8 UnicodeToCp850(U32 codePoint);
void CopyUtf8ToCp850(char *dst, size_t dstSize, const char *src);
void FormatUtf8ToCp850(char *dst, size_t dstSize, const char *fmt, ...);

// =============================================================================
#ifdef __cplusplus
}
#endif
