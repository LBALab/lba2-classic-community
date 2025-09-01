
#pragma once
#include <POLYGON/POLY.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

typedef S32 (*Fill_Jump_Fn)(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color);

extern Fill_Jump_Fn Fill_N_Table_Jumps[];
extern Fill_Jump_Fn Fill_Fog_Table_Jumps[];
extern Fill_Jump_Fn Fill_ZBuf_Table_Jumps[];
extern Fill_Jump_Fn Fill_FogZBuf_Table_Jumps[];
extern Fill_Jump_Fn Fill_NZW_Table_Jumps[];
extern Fill_Jump_Fn Fill_FogNZW_Table_Jumps[];

// =============================================================================
#ifdef __cplusplus
}
#endif
