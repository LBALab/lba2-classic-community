//--------------------------------------------------------------------------//
#include <SYSTEM/ADELINE.H>
#include <SYSTEM/DISKDIR.H>
#include <SYSTEM/LIMITS.H>

// TODO: Remove this includes on portable code
#include <windows.h>
#include <direct.h>

#include <unistd.h>
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
