//--------------------------------------------------------------------------//
#include <SYSTEM/ADELINE.H>
#include <SYSTEM/DISKDIR.H>
#include <SYSTEM/LIMITS.H>

#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <direct.h>
#include <ctype.h>

//--------------------------------------------------------------------------//
void	ChDiskDir( char *path)
{
	char 		drive[ ADELINE_MAX_DRIVE ]	;

	_splitpath(path, drive, NULL, NULL, NULL);
	_chdrive(toupper(drive[0])-'A'+1);
	chdir(path)				;
}

//--------------------------------------------------------------------------//
