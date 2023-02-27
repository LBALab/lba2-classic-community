#pragma once

#include <3D/DATAMAT.H>
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
typedef S32(Func_LongProjectPoint)(S32 x, S32 y, S32 z);

// -----------------------------------------------------------------------------
extern	Func_LongProjectPoint	*LongProjectPoint	;

#pragma	aux LongProjectPoint		"*"

extern Func_LongProjectPoint LongProjectPointIso; ///< Do Not call from C
extern Func_LongProjectPoint LongProjectPoint3D; ///< Do Not call from C

// -----------------------------------------------------------------------------
typedef void(Func_ProjectList)(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX,
                               S32 OrgY, S32 OrgZ);

// -----------------------------------------------------------------------------
extern Func_ProjectList *ProjectList;   ///< Do Not call from C
extern Func_ProjectList ProjectListIso; ///< Do Not call from C
extern Func_ProjectList *ProjectList3D; ///< Do Not call from C

// =============================================================================
#ifdef __cplusplus
}
#endif
