// -----------------------------------------------------------------------------
#ifndef RESOLUTION_X
#define RESOLUTION_X 640
#endif

#ifndef RESOLUTION_Y
#define RESOLUTION_Y 480
#endif

#ifndef RESOLUTION_DEPTH
#define RESOLUTION_DEPTH 8
#endif

#if (RESOLUTION_X & 7)
#error Horizontal resolution must be a multiple of 8
#endif

//··········································································
#ifdef	DEBUG_MALLOC
atexit(SafeErrorMallocMsg);
#endif

//··········································································
{

//··········································································
        const char    *Adeline="ADELINE";
        const char    *name=lname 	;
	char	*defname		;
	char	old_path[_MAX_PATH]=""	;
	char	old_path2[_MAX_PATH]=""	;

//··········································································
// Quiet Log
#if	((inits) & INIT_QUIET)
	QuietLog = TRUE			;
#endif

//··········································································
	getcwd(old_path, _MAX_PATH)	;

	PathConfigFile[0] = 0		;

        defname = getenv(Adeline) 	;
        if(defname)
	{
		char 	drive[ _MAX_DRIVE ]	;
		char 	dir[ _MAX_DIR ]		;
		unsigned int	n		;

		if(FileSize(defname))
		{
			_splitpath( defname, drive, dir, NULL, NULL );
		}
		else
		{
			// env var contain only directory name with no file specified
			strcpy(PathConfigFile, defname)	;
			strcat(PathConfigFile, "\\dummy.tmp");
			_splitpath(PathConfigFile, drive, dir, NULL, NULL );
		}
		_makepath(PathConfigFile, drive, dir, "", "");
		if(name)
		{
			strcat(PathConfigFile, name)	;
		}
		_chdrive(toupper(drive[0])-'A'+1);
		n = strlen(dir)-1		;
		while((dir[n]=='\\')||(dir[n]=='/'))
		{
			dir[n] = 0		;
			n--			;
		}
		getcwd(old_path2, _MAX_PATH)	;
		chdir(dir)			;
	}
	else
	{
		strcpy(PathConfigFile, old_path);
		strcat(PathConfigFile, "\\")	;
		if(name)
		{
			strcat(PathConfigFile, name);
		}

		chdir("Drivers")		;
	}

//··········································································
// LOG
#if	((inits) & INIT_LOG)
		CreateLog(PathConfigFile)	;
#endif

//··········································································
InitEvents();

// --- WINDOW ------------------------------------------------------------------
#if ((inits) & INIT_WINDOW)
  LogPuts("\nInitialising Window. Please wait...\n");

  if (!InitWindow(APPNAME)) {
    exit(1); // TODO: Implement graceful exit
  }
#endif

//··········································································
// Config File
	if(name&&!FileSize(PathConfigFile))
	{
		LogPrintf("Error: Can't find config file %s\n\n", PathConfigFile);
		exit(1);
	}

//··········································································
// CMDLINE
GetCmdLine(argc, argv);

//··········································································
// OS
	LogPuts("\nIdentifying Operating System. Please wait...\n");
	DisplayOS()			;

//··········································································
// CPU
	LogPuts("\nIdentifying CPU. Please wait...\n");

	if(!FindAndRemoveParam("/CPUNodetect"))
	{
		ProcessorIdentification();
	}

	DisplayCPU()			;

	if(ParamsCPU())
	{
		LogPuts("\nSome command Line Parameters override CPU detection.\nNew CPU parameters:\n");

		DisplayCPU()		;
	}

//··········································································
// CmdLine Parser
#ifdef	paramparser

	{
		char	cur_path[_MAX_PATH]	;

		getcwd(cur_path, _MAX_PATH)	;
		if(defname)
		{
			chdir(old_path2);
		}
		ChDiskDir(old_path)	;

	        paramparser()  		;

		ChDiskDir(cur_path)	;
	}

#endif

//··········································································
// AIL API init (for vmm_lock/timer)

	InitAIL() ;

// --- VIDEO -------------------------------------------------------------------
#if ((inits) & INIT_VIDEO)
  LogPuts("\nInitialising Video. Please wait...\n");

  if (!InitVideo()) {
    exit(1); // TODO: Implement graceful exit
  }

  if (!InitScreen()) {
    exit(1); // TODO: Implement graceful exit
  }

  if (!InitGraphics(RESOLUTION_X, RESOLUTION_Y)) {
    exit(1); // TODO: Implement graceful exit
  }
#endif

//··········································································
// Midi device

#if ((inits) & INIT_MIDI)

#ifndef	LIB_AIL

#error ADELINE: you need to include AIL.H

#endif

	LogPuts("\nInitialising Midi device. Please wait...\n");

	if( !InitMidiDriver(NULL) )		exit( 1 ) ;

#endif

//··········································································
// Sample device

#if ((inits) & INIT_SAMPLE)

#ifndef	LIB_AIL

#error ADELINE: you need to include AIL.H

#endif

	LogPuts("\nInitialising Sample device. Please wait...\n");

	if( !InitSampleDriver(NULL) )		exit( 1 ) ;

#endif

//··········································································
// Smacker

#if ((inits) & INIT_SMACKER)

#ifndef	LIB_SMACKER

#error ADELINE: you need to include SMACKER.H

#endif

	LogPuts("\nInitialising Smacker. Please wait...\n");

        // FIXME: Uncomment, now just for testing build...
	//InitSmacker()	;

#endif

//··········································································
// keyboard
#if   ((inits) & INIT_KEYB)

        InitKeyboard()  ;

#endif

//··········································································
// svga
#ifndef _WIN32
#if   ((inits) & (INIT_SVGA|INIT_VESA))

#ifndef	LIB_SVGA

#error ADELINE: you need to include SVGA.H

#endif

	LogPuts("\nInitialising SVGA device. Please wait...\n");

	if(ParamsSvga())
	{
		LogPuts("\nSome command Line Parameters override SVGA detection.\n");
	}

        if(InitGraphSvga(RESOLUTION_X, RESOLUTION_Y, RESOLUTION_DEPTH))
        {
        	exit(1)	;
        }

#endif
#endif

//··········································································
// mouse
#if   ((inits) & INIT_MOUSE)

        InitMouse()	;
#endif

//··········································································
// init Timer

#if   ((inits) & INIT_TIMER)
  InitTimer();
#endif

//··········································································
	if(defname)
	{
		chdir(old_path2);
	}

	ChDiskDir(old_path)	;

//··········································································
// Quiet Log
#if	!((inits) & INIT_QUIET)
	QuietLog = TRUE			;
#endif

//··········································································
// DefFile
#if	((inits) & INIT_DEFFILE)

#ifndef	ibuffer

#error ADELINE: you need to define ibuffer

#endif

#ifndef	ibuffersize

#error ADELINE: you need to define ibuffersize

#endif
	DefFileBufferInit(PathConfigFile, (void*)(ibuffer), ibuffersize);
#endif

//··········································································
}

//··········································································
