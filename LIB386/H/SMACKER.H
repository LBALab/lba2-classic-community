//──────────────────────────────────────────────────────────────────────────
#ifndef	LIB_SMACKER
#define	LIB_SMACKER

//──────────────────────────────────────────────────────────────────────────
#ifndef	_BUILDING_LIB

#ifdef	_WIN32

#pragma library ("SMACKW32.LIB")

#ifdef	__SW_3R
#pragma library ("NT_SMKR.LIB")
#else//	__SW_3R
#pragma library ("NT_SMKS.LIB")
#endif//__SW_3R

#else//	_WIN32

#pragma library ("SMACK.LIB")

#ifdef	__SW_3R
#pragma library ("DOS_SMKR.LIB")
#else//	__SW_3R
#pragma library ("DOS_SMKS.LIB")
#endif//__SW_3R

#endif//_WIN32

#endif//_BUILDING_LIB

//──────────────────────────────────────────────────────────────────────────
#ifdef MVIDEO_USE_SMACKER
#include	<SMACKER/SMACK.H>
#include	<SMACKER/SMACKER.H>
#else
// TODO: Remove comment, enable warning
//#warning "Compiling without any Motion Video playback support!"
#endif

//──────────────────────────────────────────────────────────────────────────
#endif//LIB_SMACKER

//──────────────────────────────────────────────────────────────────────────
