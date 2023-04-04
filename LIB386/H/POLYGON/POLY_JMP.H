
#pragma once
#include <POLYGON/POLY.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

typedef S32 (*Fill_Jump_Fn)(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color);

extern Fill_Jump_Fn Fill_N_Table_Jumps[];

// =============================================================================
#ifdef __cplusplus
}
#endif
