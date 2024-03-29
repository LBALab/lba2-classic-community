//****************************************************************************
#ifndef	LIB_SVGA_CPYBLOCI
#define	LIB_SVGA_CPYBLOCI

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif

//****************************************************************************
void	CopyBlockIncrust(S32 x0, S32 y0, S32 x1, S32 y1, void *src, S32 xd, S32 yd, void *dst);

#pragma aux CopyBlockIncrust	"*"			\
	parm caller	[eax] [ebx] [ecx] [edx] [esi]	\
	modify		[edi]

//****************************************************************************
#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_SVGA_CPYBLOCI

//****************************************************************************

