#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#include <stdbool.h>
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

// Compare two strings that name the same thing but may not share an encoding:
// ASCII is compared case-insensitively, and any run of bytes >= 0x80 on one
// side matches any such run on the other. So "Fran\x87ais" (a retail config,
// CP437/CP850) equals "Fran\xC3\xA7ais" (UTF-8, which is what this source tree
// is), while "Francais" equals neither -- a plain letter is not an accent.
//
// For short names from a fixed set whose ASCII letters already tell them apart.
// Not a general collation: it cannot tell two names apart if they differ only
// in their accented characters.
bool EqualsIgnoringCaseAndEncoding(const char *a, const char *b);

// =============================================================================
#ifdef __cplusplus
}
#endif
