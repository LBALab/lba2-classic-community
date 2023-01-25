#include <SYSTEM/INITADEL.H>

#include <AIL/TIMER.H>
#include <SVGA/INITMODE.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/CMDLINE.H>
#include <SYSTEM/CPU.H>
#include <SYSTEM/DEFFILE.H>
#include <SYSTEM/DISKDIR.H>
#include <SYSTEM/DISPCPU.H>
#include <SYSTEM/DISPOS.H>
#include <SYSTEM/EVENTS.H>
#include <SYSTEM/EXIT.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/LZ.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/WINDOW.H>


// FIXME: Remove, here only for testing...
#pragma pack(8) // n = 4
#ifdef _WIN32
//#include <mmsystem.h>
//#include <winbase.h>
//#include <windows.h>
//#include <windowsx.h>
#include <direct.h>
#endif //_WIN32
#pragma pack(1)

#include <ctype.h>
#include <unistd.h>

// -----------------------------------------------------------------------------
#ifdef DEMO
#define APPNAME "LBA2 Twinsen's Odyssey Demo"
#else // DEMO
#define APPNAME "LBA2"
#endif // DEMO

#define ibuffer ScreenAux
#define ibuffersize (640 * 480 + RECOVER_AREA)
#define lname "lba2.cfg"

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

// ··········································································
#ifdef DEBUG_MALLOC
atexit(SafeErrorMallocMsg);
#endif

// ··········································································
void InitAdeline(S32 argc, char *argv[]) {

  // ··········································································
  const char *Adeline = "ADELINE";
  const char *name = lname;
  char *defname;
  char old_path[_MAX_PATH] = "";
  char old_path2[_MAX_PATH] = "";

  // ··········································································
  {
    getcwd(old_path, _MAX_PATH); // old_path is current directory

    PathConfigFile[0] = 0;

    // If env var ADELINE is specified...
    defname = getenv(Adeline);
    if (defname) {
      char drive[_MAX_DRIVE];
      char dir[_MAX_DIR];
      unsigned int n;

      // Guarantee 'drive' and 'dir' are filled
      if (FileSize(defname)) {
        _splitpath(defname, drive, dir, NULL, NULL);
      } else {
        // env var contain only directory name with no file specified
        strcpy(PathConfigFile, defname);
        strcat(PathConfigFile, "\\dummy.tmp");
        _splitpath(PathConfigFile, drive, dir, NULL, NULL);
      }

      // Set PathConfigFile to full path
      _makepath(PathConfigFile, drive, dir, "", "");

      // If config file name specified, add it to PathConfigFile
      if (name) {
        strcat(PathConfigFile, name);
      }

      // Change drive to one from env path, keeps current directory
      _chdrive(toupper(drive[0]) - 'A' + 1);

      // Set dir to path without ending path separator '\' or '/'
      n = strlen(dir) - 1;
      while ((dir[n] == '\\') || (dir[n] == '/')) {
        dir[n] = 0;
        n--;
      }

      getcwd(old_path2,
             _MAX_PATH); // old_path2 is current dir with the new drive
      chdir(dir);        // change dir to env path
    } else {
      // Set PathConfigFile to current running path + config file name (if
      // specified)
      strcpy(PathConfigFile, old_path);
      strcat(PathConfigFile, "\\");
      if (name) {
        strcat(PathConfigFile, name);
      }

      chdir("Drivers"); // ???????????
    }

    // ===> Here we have PathConfigFile with full path to config file
    // ===> Here the current path is the path that should contain the config
    //      file + "Drivers" (????)
  }

  // ··········································································
  //  LOG
  CreateLog(PathConfigFile);

  // ··········································································
  InitEvents();

  // --- WINDOW ----------------------------------------------------------------
  {
    LogPuts("\nInitialising Window. Please wait...\n");
    if (!InitWindow(APPNAME)) {
      exit(1); // TODO: Implement graceful exit
    }
  }

  // ··········································································
  //  Config File
  if (name && !FileSize(PathConfigFile)) {
    LogPrintf("Error: Can't find config file %s\n\n", PathConfigFile);
    exit(1);
  }

  // ··········································································
  //  CMDLINE
  GetCmdLine(argc, argv);

  // ··········································································
  //  OS
  LogPuts("\nIdentifying Operating System. Please wait...\n");
  DisplayOS();

  // ··········································································
  //  CPU
  LogPuts("\nIdentifying CPU. Please wait...\n");

  if (!FindAndRemoveParam("/CPUNodetect")) {
    ProcessorIdentification();
  }

  DisplayCPU();

  if (ParamsCPU()) {
    LogPuts("\nSome command Line Parameters override CPU detection.\nNew CPU "
            "parameters:\n");

    DisplayCPU();
  }

// ··········································································
//  CmdLine Parser
#ifdef paramparser

  {
    char cur_path[_MAX_PATH];

    getcwd(cur_path, _MAX_PATH);
    if (defname) {
      chdir(old_path2);
    }
    ChDiskDir(old_path);

    paramparser();

    ChDiskDir(cur_path);
  }

#endif

  // ··········································································
  //  AIL API init (for vmm_lock/timer)

  InitAIL();

  // --- VIDEO
  // -------------------------------------------------------------------

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

  // ··········································································
  //  Midi device

#if ((inits)&INIT_MIDI)

#ifndef LIB_AIL

#error ADELINE: you need to include AIL.H

#endif

  LogPuts("\nInitialising Midi device. Please wait...\n");

  if (!InitMidiDriver(NULL))
    exit(1);

#endif

  // ··········································································
  //  Sample device
  /*
  #ifndef LIB_AIL

  #error ADELINE: you need to include AIL.H

  #endif

    LogPuts("\nInitialising Sample device. Please wait...\n");

    if (!InitSampleDriver(NULL))
      exit(1);
  */
  // ··········································································
  //  Smacker
  /*
  #ifndef LIB_SMACKER

  #error ADELINE: you need to include SMACKER.H

  #endif

    LogPuts("\nInitialising Smacker. Please wait...\n");

    InitSmacker()	;
  */
  // ··········································································
  //  keyboard

  InitKeyboard();

// ··········································································
//  mouse
#if ((inits)&INIT_MOUSE)

  InitMouse();
#endif

  // ··········································································
  //  init Timer

  InitTimer();

  // ··········································································
  if (defname) {
    chdir(old_path2);
  }

  ChDiskDir(old_path);

  // ··········································································
  //  DefFile

  DefFileBufferInit(PathConfigFile, (void *)(ibuffer), ibuffersize);

  // ··········································································
}

// ··········································································
