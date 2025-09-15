//****************************************************************************
#ifndef	LIB_SYSTEM_FASTCPY
#define	LIB_SYSTEM_FASTCPY

#include <SYSTEM/ADELINE_TYPES.H>

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif

#pragma aux	Arg_FastCopy	"*"	\
	parm caller	[edi] [esi] [ecx]	\
	modify	[eax ebx edx]

#pragma	aux (Arg_FastCopy)	Func_FastCopy

//****************************************************************************
extern	void FastCopy(void *dst, void *src, U32 len)	;
extern void CopyScreenColour(U8 * source, U8 * destination, U8 colour) ;

#pragma aux FastCopy	"*"

//****************************************************************************
#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_SYSTEM_FASTCPY

//****************************************************************************

