#pragma once

#include <POLYGON/POLY.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
  S32 V_X0;
  S32 V_Y0;
  S32 V_Z0;
  U16 V_MapU;
  U16 V_MapV;
  U16 V_Light;
  U16 V_Dummy;
} STRUC_CLIPVERTEX;
#pragma pack(pop)

// -----------------------------------------------------------------------------
///< @return -1 if negative half-space
extern U32 ClipperZ(STRUC_CLIPVERTEX dst[], STRUC_CLIPVERTEX src[],
                    U32 nbvertex, S32 zclip, S32 flag);

// =============================================================================
#ifdef __cplusplus
}
#endif
