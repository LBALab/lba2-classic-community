//----------------------------------------------------------------------------
#include	<SYSTEM/ADELINE.H>
#include	<SYSTEM/LOGPRINT.H>
#include <SYSTEM/LIMITS.H>

#include	<stdarg.h>
#include	<stdio.h>
#include	<string.h>

//----------------------------------------------------------------------------
	U32	QuietLog = FALSE	;
static	char	LogName[ADELINE_MAX_PATH]=""	;
static	char	LogStr[256]		;

//----------------------------------------------------------------------------
void	CreateLog(const char *filePath)
{
	FILE	*logfile 		;

	strncpy(LogName, filePath, ADELINE_MAX_PATH);
	LogName[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee

	logfile	= fopen(LogName, "wb")	;
	if(!logfile)
	{
		LogName[0] = 0		;
	}
	else
	{
		fclose(logfile)		;
	}

	if (logfile) {
		LogPrintf("Creating log file at '%s'...\n", LogName);
	}
	else {
		LogPrintf("Warning: Unable to create log file at '%s', "
			  "outputting only to console...\n", LogName);
	}
}

//----------------------------------------------------------------------------
void	LogPrintf(const char *format, ...)
{
	char	*ptr	;
	va_list	arglist	;

	va_start(arglist, format);

	vsprintf(LogStr, format, arglist);

	for(ptr=LogStr; *ptr; ptr++)
	{
		if(*ptr=='\n')
		{
			memmove(ptr+2, ptr+1, strlen(ptr))	;
			*ptr++ = '\r'				;
			*ptr   = '\n'				;
		}
	}

	va_end(arglist);

	if(!QuietLog)
	{
		printf("%s", LogStr);
	}

	if(*LogName)
	{
		FILE	*logfile ;

		logfile	= fopen(LogName, "ab+");
		if(logfile)
		{
			fprintf(logfile, "%s", LogStr);
			fclose(logfile)	;
		}
	}
}

//----------------------------------------------------------------------------
void	LogPuts(const char *string)
{
	LogPrintf("%s\n", string);
}

//----------------------------------------------------------------------------
