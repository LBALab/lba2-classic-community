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

// -----------------------------------------------------------------------------
#define CoulText(ink, paper)                                                   \
  {                                                                            \
    TextInk = (U8)(ink);                                                       \
    TextPaper = (U8)(paper);                                                   \
  }

// =============================================================================
#ifdef __cplusplus
}
#endif
