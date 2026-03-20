#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
extern U32 SizeChar;
extern U8 TextInk;
extern U8 TextPaper;

// -----------------------------------------------------------------------------
extern void AffString(S32 x, S32 y, char *str);

/** Draw string to an arbitrary 8-bit buffer (e.g. console overlay). pitch = bytes per row. */
void AffStringToBuffer(U8 *dst, U32 pitch, S32 x, S32 y, const char *str, U8 ink);

// -----------------------------------------------------------------------------
#define CoulText(ink, paper)     \
    {                            \
        TextInk = (U8)(ink);     \
        TextPaper = (U8)(paper); \
    }

// =============================================================================
#ifdef __cplusplus
}
#endif
