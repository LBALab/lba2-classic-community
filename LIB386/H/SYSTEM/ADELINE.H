//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_ADELINE
#define LIB_SYSTEM_ADELINE

//──────────────────────────────────────────────────────────────────────────
#pragma pack(8)   // n = 4
#ifdef	_WIN32
#include	<windows.h>
#include 	<windowsx.h>
#include	<winbase.h>
#include 	<mmsystem.h>
#ifndef	_NO_DDRAW
#include 	<ddraw.h>
#endif//_NO_DDRAW
#endif//_WIN32
#pragma pack(1)

#include "ADELINE_TYPES.H"

//──────────────────────────────────────────────────────────────────────────
#ifndef TYPEDEFS
#define TYPEDEFS

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
#ifdef  _WIN32

//──────────────────────────────────────────────────────────────────────────
typedef	S32 (WINDOW_PROC)(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#pragma	aux	(Flat_Name)	WINDOW_PROC

//──────────────────────────────────────────────────────────────────────────
typedef	WINDOW_PROC * WINDOW_PROC_PTR	;

#pragma	aux	(Flat_Name)	WINDOW_PROC_PTR

//──────────────────────────────────────────────────────────────────────────
#endif//_WIN32

//──────────────────────────────────────────────────────────────────────────
#ifdef	_WIN32

//──────────────────────────────────────────────────────────────────────────
#define	AIL_vmm_lock(a, b)
#define	AIL_vmm_unlock(a, b)

//──────────────────────────────────────────────────────────────────────────
#else//	_WIN32

//──────────────────────────────────────────────────────────────────────────
//#define		ManageTime()
#define	ManageSystem()	{ ManageEvents(); ManageTime(); } // to get time

//──────────────────────────────────────────────────────────────────────────
#endif//_WIN32

//──────────────────────────────────────────────────────────────────────────
#ifndef _WIN32
#ifndef  DPMI
#define  DPMI	// we use DOS/4GW... mainly for DLLLOAD.C (Miles)
#endif// DPMI
#endif//_WIN32

//──────────────────────────────────────────────────────────────────────────
#endif//TYPEDEFS

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif//LIB_SYSTEM_ADELINE

//──────────────────────────────────────────────────────────────────────────
