#include "INITADEL.H"

#include "C_EXTERN.H"
#include "DIRECTORIES.H"
#include "RES_SWITCH.H" /* Res_LoadBootDimensions — CLI > cfg > default */

#include "AIL/COMMON.H"
#include "CONTROL.H"
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
#include "SYSTEM/RESOLUTION.H"
#include "SYSTEM/TIMER.H"
#include "SYSTEM/WINDOW.H"

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
int WriteEmbeddedDefaultLba2Cfg(const char *destPath);
#ifdef __cplusplus
}
#endif
#include <unistd.h>

// -----------------------------------------------------------------------------
#include "BUILD_INFO.h"
#ifdef DEMO
#define APPNAME LBA2_PRODUCT_NAME_DEMO " " LBA2_VERSION_STRING
#else // DEMO
#define APPNAME LBA2_PRODUCT_NAME " " LBA2_VERSION_STRING
#endif // DEMO

#define ibuffer ScreenAux
#define ibuffersize (RESOLUTION_X * RESOLUTION_Y + RECOVER_AREA)

// -----------------------------------------------------------------------------
#ifndef RESOLUTION_DEPTH
#define RESOLUTION_DEPTH 8
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
    {
        LogPuts("\nInitialising Event system. Please wait...\n");
        if (!InitEvents()) {
            exit(1); // TODO: Implement graceful exit
        }
    }

    // ··········································································
    {
        LogPuts("\nInitialising Joystick. Please wait...\n");
        InitJoystick();
    }

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
        LogPuts("Config file not found! Copying default from assets folder...");

        char PathDefaultConfigFile[ADELINE_MAX_PATH];
        GetDefaultCfgPath(PathDefaultConfigFile, ADELINE_MAX_PATH, CFG_NAME);
        if (!ExistsFileOrDir(PathDefaultConfigFile)) {
            LogPuts("Default config not in assets folder; writing embedded template...");

            if (!WriteEmbeddedDefaultLba2Cfg(PathConfigFile)) {
                LogPrintf("Error: Can't write embedded default config to '%s'\n\n",
                          PathConfigFile);
                exit(1);
            }
        } else {
            const bool copyResult = Copy(PathDefaultConfigFile, PathConfigFile);
            if (!copyResult) {
                LogPrintf("Error: Can't copy config file from '%s' to '%s'\n\n",
                          PathDefaultConfigFile, PathConfigFile);
                exit(1);
            }
        }
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

    /* --no-audio (harness): skip the SDL audio subsystem entirely. Avoids the
       SDL dummy driver's nanosleep pacing — ~58% of sys time in projection_demo
       on a WSL2 setup. Player builds always init audio; this gate is only
       reached when the harness explicitly opts out. Call sites under
       LIB386/AIL/SDL/ gate on Sample_Driver_Enabled / non-NULL stream, so
       playback paths become silent no-ops when audio isn't up. */
    if (!Control_NoAudio()) {
        InitAIL(); // TODO: Reorganize/reposition closer to sound subsystem
    }

    // --- VIDEO
    // -------------------------------------------------------------------

    LogPuts("\nInitialising Video. Please wait...\n");

    if (!InitVideo()) {
        exit(1); // TODO: Implement graceful exit
    }

    if (!InitScreen()) {
        exit(1); // TODO: Implement graceful exit
    }

    {
        /* Same boot-resolution resolver PERSO.CPP main uses for
           Mem_ConfigureScreenBuffers — precedence is
              --resolution CLI > lba2.cfg ResolutionX/Y > compile-time default.
           PERSO.CPP already called GetCfgPath / Res_LoadBootDimensions,
           so the file read here re-reads a known-good cfg (DefFileBufferInit
           is idempotent — module statics are fully re-anchored on re-init).
           Both call sites MUST stay in sync or MainBuffer (sized in PERSO)
           ends up at a different resolution than Log/Screen (sized here),
           which makes scene rendering clip to the smaller dimensions. */
        U32 reqResX, reqResY;
        Res_LoadBootDimensions(&reqResX, &reqResY);
        if (!InitGraphics(reqResX, reqResY)) {
            exit(1); // TODO: Implement graceful exit
        }
    }

    // ··········································································
    //  Midi device

#if ((inits) & INIT_MIDI)
#ifndef LIB_AIL
#error ADELINE: you need to include AIL.H
#endif
    LogPuts("\nInitialising Midi device. Please wait...\n");
    if (!InitMidiDriver(NULL))
        exit(1);
#endif

    // ··········································································
    //  Sample device
    if (!Control_NoAudio()) {
#ifndef LIB_AIL
#error ADELINE: you need to include AIL.H
#endif
        LogPuts("\nInitialising Sample device. Please wait...\n");
        if (!InitSampleDriver(NULL)) {
            /* No usable audio output (host has no audio device, SDL_AUDIODRIVER
               points at something invalid, the audio subsystem failed to come
               up earlier). Previously this exit(1)'d the process; now we log
               and continue with audio disabled. Every audio entry point in
               LIB386/AIL/SDL/{SAMPLE,STREAM,VIDEO_AUDIO_SDL}.CPP already gates
               on Sample_Driver_Enabled / a non-NULL stream, so playback calls
               become silent no-ops rather than UB. The harness uses this to
               bypass the SDL dummy driver's nanosleep pacing (~58% of sys time
               in projection_demo); a player whose audio device fails just
               loses sound instead of being unable to launch the game. */
            LogPuts("\nWarning: no audio output — running silently.\n");
        }
    }

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
