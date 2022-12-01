//──────────────────────────────────────────────────────────────────────────
#ifndef	LIB_SYSTEM_A_MALLOC
#define LIB_SYSTEM_A_MALLOC

//──────────────────────────────────────────────────────────────────────────
#include	<SYSTEM/S_MALLOC.H>
#include	<SYSTEM/N_MALLOC.H>

//──────────────────────────────────────────────────────────────────────────
#ifdef	__cplusplus
extern	"C"	{
#endif

//──────────────────────────────────────────────────────────────────────────
#define Malloc			NormMalloc
#define DosMalloc		NormDosMalloc
#define DosFree 		NormDosFree

//──────────────────────────────────────────────────────────────────────────
#ifdef  DEBUG_MALLOC

#define Mshrink(a, b)           SafeMshrink(a, b, __FILE__, __LINE__)
#define Free(a)                 SafeFree(a, __FILE__, __LINE__)

//──────────────────────────────────────────────────────────────────────────
#else// DEBUG_MALLOC

#define Mshrink(a, b)           NormMshrink(a, b)
#define Free(a)                 NormFree(a)

#endif//DEBUG_MALLOC

//──────────────────────────────────────────────────────────────────────────
#ifdef	_WIN32

#define	SmartMalloc(size)	Malloc(size)

#else// _WIN32

extern	void	*SmartMalloc(S32 size)		;

#endif//_WIN32

//──────────────────────────────────────────────────────────────────────────
#ifdef	__cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif//LIB_SYSTEM_N_MALLOC

//──────────────────────────────────────────────────────────────────────────
