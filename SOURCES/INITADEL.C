#include "INITADEL.H"

#include "C_EXTERN.H"
#include "DIRECTORIES.H"

#include "AIL/TIMER.H"
#include "SVGA/INITMODE.H"
#include "SVGA/SCREEN.H"
#include "SVGA/VIDEO.H"
#include "SYSTEM/CMDLINE.H"
#include "SYSTEM/CPU.H"
#include "SYSTEM/DEFFILE.H"
#include "SYSTEM/DISPOS.H"
#include "SYSTEM/EVENTS.H"
#include "SYSTEM/EXIT.H"
#include "SYSTEM/FILES.H"
#include "SYSTEM/KEYBOARD.H"
#include "SYSTEM/LOGPRINT.H"
#include "SYSTEM/LZ.H"
#include "SYSTEM/MOUSE.H"
#include "SYSTEM/TIMER.H"
#include "SYSTEM/WINDOW.H"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// -----------------------------------------------------------------------------
#ifdef DEMO
#define APPNAME "LBA2 Twinsen's Odyssey Demo"
#else // DEMO
#define APPNAME "LBA2"
#endif // DEMO

#define ibuffer ScreenAux
#define ibuffersize (640 * 480 + RECOVER_AREA)

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

  {
    char resFolderPath[ADELINE_MAX_PATH] = "";
    char saveFolderPath[ADELINE_MAX_PATH] = "";
    char cfgFolderPath[ADELINE_MAX_PATH] = "";
    char logFilePath[ADELINE_MAX_PATH] = "";

    GetResPath(resFolderPath, ADELINE_MAX_PATH, NULL);
    GetSavePath(saveFolderPath, ADELINE_MAX_PATH, NULL);
    GetCfgPath(cfgFolderPath, ADELINE_MAX_PATH, NULL);

    GetLogPath(logFilePath, ADELINE_MAX_PATH, LOG_NAME);
    CreateLog(logFilePath);
    LogPuts("Starting game...");
    LogPuts("Paths used:");
    LogPrintf("\t* Assets:\t%s\n"
              "\t* Saves:\t%s\n"
              "\t* Config:\t%s\n",
              resFolderPath, saveFolderPath, cfgFolderPath);
  }

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
  GetCfgPath(PathConfigFile, ADELINE_MAX_PATH, CFG_NAME);
  if (!ExistsFileOrDir(PathConfigFile)) {
    // TODO: Create default config file if does not already exists
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
  // TODO: Remove when all ASM is ported to C
  if (!FindAndRemoveParam("/CPUNodetect")) {
    //ProcessorIdentification();
    ProcessorSignature.FPU = 1;
    ProcessorSignature.Family = 5;
    ProcessorSignature.Model = 4;
    ProcessorSignature.Manufacturer = 1;
    ProcessorFeatureFlags.MMX = 0;
  }

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
  InitMouse();

  // ··········································································
  //  init Timer

  InitTimer();

  // ··········································································
  //chdir(resFolderPath);

  // ··········································································
  //  DefFile

  DefFileBufferInit(PathConfigFile, (void *)(ibuffer), ibuffersize);

  // ··········································································
}

// ··········································································
