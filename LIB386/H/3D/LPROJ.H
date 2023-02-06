//****************************************************************************
#ifndef	LIB_3D_LPROJ
#define	LIB_3D_LPROJ

//****************************************************************************
#include	"DATAMAT.H"

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif

//****************************************************************************
typedef	S32 (Func_LongProjectPoint)(S32 x, S32 y, S32 z)	;

//****************************************************************************
//extern	Func_LongProjectPoint	*LongProjectPoint	;

#define LongProjectPoint LongProjectPoint_C
S32 LongProjectPoint_C(S32 x, S32 y, S32 z);

//****************************************************************************
extern	Func_LongProjectPoint	LongProjectPointIso	;

//****************************************************************************
extern	Func_LongProjectPoint	*LongProjectPoint3D	;

//****************************************************************************
// These functions should not be called directly but through the above pointer
// which auto-initalize to point to the right routine at runtime.
extern	Func_LongProjectPoint	LongProjectPoint3DI	;
extern	Func_LongProjectPoint	LongProjectPoint3DF	;

//****************************************************************************
// This function is for the lib INTERNAL USE ONLY
extern	Func_LongProjectPoint	LongProjectPointInit	;

//****************************************************************************
//****************************************************************************
typedef	S32 (Func_ProjectList)(	TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt,
				S32 OrgX, S32 OrgY, S32 OrgZ)	;
//WARNING: OrgX, OrgY, OrgZ pushed on the stack

//****************************************************************************
extern	Func_ProjectList	*ProjectList	;

//****************************************************************************
extern	Func_ProjectList	ProjectListIso	;
extern	Func_ProjectList	*ProjectList3D	;

//****************************************************************************
// These functions should not be called directly but through the above pointer
// which auto-initalize to point to the right routine at runtime.
extern	Func_ProjectList	ProjectList3DI	;
extern	Func_ProjectList	ProjectList3DF	;
extern	Func_ProjectList	ProjectList3DMF	;

//****************************************************************************
// This function is for the lib INTERNAL USE ONLY
extern	Func_ProjectList	ProjectListInit	;

//****************************************************************************
#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_3D_LPROJ

//****************************************************************************

