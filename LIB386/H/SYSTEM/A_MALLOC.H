//──────────────────────────────────────────────────────────────────────────
#ifndef	LIB_SYSTEM_A_MALLOC
#define LIB_SYSTEM_A_MALLOC

//──────────────────────────────────────────────────────────────────────────
#include	<SYSTEM/S_MALLOC.H>
#include	<SYSTEM/N_MALLOC.H>
#include <SYSTEM/ADELINE_TYPES.H>

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
#define	SmartMalloc(size)	Malloc(size)

//──────────────────────────────────────────────────────────────────────────
#ifdef	__cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif//LIB_SYSTEM_N_MALLOC

//──────────────────────────────────────────────────────────────────────────
