//****************************************************************************
#ifndef	LIB_3D_IMATSTD
#define	LIB_3D_IMATSTD

//****************************************************************************
#include	"DATAMAT.H"

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif

//****************************************************************************
typedef	void (Func_InitMatrix)(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma) ;

//extern	Func_InitMatrix	*InitMatrixStd	;
#define InitMatrixStd InitMatrixStd_C
void InitMatrixStd_C(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma);

//****************************************************************************
// These should not be called directly but through the above pointer
extern	Func_InitMatrix	InitMatrixStdI	;
extern	Func_InitMatrix	InitMatrixStdF	;
extern	Func_InitMatrix	InitMatrixStdM	;

//****************************************************************************
#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_3D_IMATSTD

//****************************************************************************

