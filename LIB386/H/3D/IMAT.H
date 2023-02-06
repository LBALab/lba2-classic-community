//****************************************************************************
#ifndef	LIB_3D_IMAT
#define	LIB_3D_IMAT

//****************************************************************************
#include	"DATAMAT.H"

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif

//****************************************************************************
typedef	void (Func_InitMatrix)(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma) ;

#pragma	aux Arg_InitMatrix	"*"		\
	parm caller		[edi] [eax] [ebx] [ecx]	\
	modify exact	[eax ebx ecx edx esi]

#pragma	aux (Arg_InitMatrix)	Func_InitMatrix

//****************************************************************************
extern	Func_InitMatrix	*InitMatrix	;

#pragma	aux	InitMatrix	"*"

//****************************************************************************

#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_3D_IMAT

//****************************************************************************
