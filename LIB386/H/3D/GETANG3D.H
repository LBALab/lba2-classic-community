//****************************************************************************
#ifndef	LIB_3D_GETANG3D
#define	LIB_3D_GETANG3D

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif

//****************************************************************************
extern	S32 	GetAngleVector3D( S32 x, S32 y, S32 z )	;

#pragma aux GetAngleVector3D	"*"			\
	parm caller		[eax] [ebx] [ecx] 		\
	modify 		[edx esi]

//****************************************************************************
#define	GetAngle3D(x0, y0, z0, x1, y1, z1)	GetAngleVector3D((x1)-(x0), (y1)-(y0), (z1)-(z0))

//****************************************************************************
#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_3D_GETANG3D

//****************************************************************************
