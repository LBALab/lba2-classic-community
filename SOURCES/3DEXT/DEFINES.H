#include <stdlib.h>
#include <stdio.h>
#include <math.h>
//#include <mem.h>
#include <string.h>
//#include <dos.h>

#include <SYSTEM.H>
#include <SVGA.H>
#include <3D.H>
#include <OBJECT.H>
#include <POLYGON.H>

// Just because Ink_Text et Ink_Paper are not LONG in Lib
#undef	WHITE
#define		WHITE	254

/*---------------- The End: error ------------------*/
#define ERROR_NOT_FOUND_FILE    0
#define NOT_ENOUGH_MEM          1
#define PROGRAM_OK              2
#define NAME_NOT_FOUND          3
/*---------------------------------------------------*/

#ifdef	MOUNFRAC
typedef struct {

	T_OBJ_3D	Obj ;			// lib struct

	T_REAL_VALUE	RealAngle;		// synchro de la rotation

	S32		XMin ;			// ZV
	S32		YMin ;
	S32		ZMin ;
	S32		XMax ;
	S32		YMax ;
	S32		ZMax ;


		} 	T_OBJET;
#endif

/*---------------------------------------------------*/

#include "EXTERN.H"

