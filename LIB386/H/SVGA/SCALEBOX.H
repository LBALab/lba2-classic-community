//****************************************************************************
#ifndef	LIB_SVGA_SCALEBOX
#define	LIB_SVGA_SCALEBOX

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif

//****************************************************************************
void	ScaleBox(	S32 xs0, S32 ys0, S32 xs1, S32 ys1, void *ptrs,
			S32 xd0, S32 yd0, S32 xd1, S32 yd1, void *ptrd	)	;

#pragma aux	ScaleBox	"*"	\
	parm caller [edx] [ecx] [eax] [edi] [esi] [ebx]

//****************************************************************************
#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_SVGA_SCALEBOX

//****************************************************************************

