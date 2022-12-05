//--------------------------------------------------------------------------//
#include 	<SYSTEM/ADELINE.H>
#include 	<SYSTEM/FILENAME.H>
#include 	<SYSTEM/FILECOPY.H>
#include 	<SYSTEM/FILEBAK.H>

#include	<stdlib.h>
#include	<string.h>

//--------------------------------------------------------------------------//
S32	CopyBak( char *name )
{
	char	bakfname[_MAX_PATH] 	;

	strcpy( bakfname, name ) 	;
	AddExt( bakfname, ".BAK" ) 	;

	return Copy( name, bakfname ) 	;
}


//--------------------------------------------------------------------------//
