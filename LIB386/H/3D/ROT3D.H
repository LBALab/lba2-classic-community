//****************************************************************************
#ifndef	LIB_3D_ROT3D
#define	LIB_3D_ROT3D

//****************************************************************************
#include	"LROT3D.H"

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif

//****************************************************************************
typedef	void (Func_RotatePoint)(TYPE_MAT *Mat, S32 x, S32 y, S32 z) ;

#pragma aux Arg_RotatePoint	"*"	       	\
	parm caller	[esi] [eax] [ebx] [ecx]	\
	modify exact	[eax ebx ecx edx edi]

#pragma	aux (Arg_RotatePoint)	Func_RotatePoint

//****************************************************************************
extern	Func_RotatePoint	*RotatePoint	;
extern	Func_RotatePoint	*RotatePointNoMMX;

//****************************************************************************
#define	WorldRotatePoint(x, y, z) RotatePoint(&MatriceWorld, x, y, z)

//****************************************************************************
#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_3D_ROT3D

//****************************************************************************

