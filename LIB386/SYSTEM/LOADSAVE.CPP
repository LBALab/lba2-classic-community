//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/LOADSAVE.H>

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LOGPRINT.H>

#include <stdlib.h>
#include <limits.h>

//──────────────────────────────────────────────────────────────────────────
U32	Load( char *name, void *buffer )
{
	S32	handle	;
	U32	size	;

	size = FileSize(name)		;
	if (!size)	return 0	;

	handle = OpenRead(name)		;
	if (handle)
	{
		size = Read(handle, buffer, size) 	;
		Close(handle)				;
		return size				;
	}
	return 0;
}
//──────────────────────────────────────────────────────────────────────────
S32	LoadSize( char *name, void *buffer, S32 size )
{
	S32	handle				;
	S32	maxsize, sizeread, sizetoread	;

	maxsize = FileSize( name )	;
	if ( !maxsize )	return(0)	;

	handle = OpenRead( name )	;
	if ( handle )
	{
		sizetoread = ade_min(maxsize, size)		;
		sizeread = Read( handle, buffer, sizetoread )	;
		Close( handle )					;
		if(sizeread < sizetoread)
		{
			if(sizeread < maxsize)
			{
				// error reading
				sizeread = LONG_MIN		;
			}
		}
		else
		{
			if(maxsize > size)
			{
				// file too large by...
				sizeread -= maxsize		;
			}
		}
		return( sizeread )				;
	}
	return( 0 )	;
}
//──────────────────────────────────────────────────────────────────────────
U32	Save( char *name, void *buffer, U32 size )
{
	S32	handle		;
	U32	written = 0	;

	handle = OpenWrite( name )	;
	if( handle )
	{
		written = Write( handle, buffer, size )	;
		Close( handle )				;
	}
	else
	{
		LogPrintf("Warning: Unable to save game to file '%s'\n", name);
	}

	return( written == size )	;
}

//──────────────────────────────────────────────────────────────────────────

