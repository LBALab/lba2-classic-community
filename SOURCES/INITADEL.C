#include "INITADEL.H"

#include "C_EXTERN.H"
#include "DIRECTORIES.H"
#include "PERSO.H"      /* BootFatal — fatal exit for init failures */
#include "RES_SWITCH.H" /* Res_LoadBootDimensions — CLI > cfg > default */

#include "AIL/COMMON.H"
#include "CONSOLE/CONSOLE.H" /* console buffer sink printer adapter */
#include "CONTROL.H"
#include "JOYSTICK.H"
#include <SYSTEM/LOG.H>
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

#include <SDL3/SDL_cpuinfo.h> /* SDL_GetNumLogicalCPUCores, SDL_GetSystemRAM */

#if defined(_WIN32)
#define LOG_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
#define LOG_PLATFORM_NAME "macOS"
#elif defined(__linux__)
#define LOG_PLATFORM_NAME "Linux"
#else
#define LOG_PLATFORM_NAME "Unknown"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define LOG_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define LOG_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define LOG_ARCH_NAME "arm64"
#elif defined(__arm__)
#define LOG_ARCH_NAME "arm"
#else
#define LOG_ARCH_NAME "unknown"
#endif

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

/* Adapter handed to the console buffer sink so the log core (now in LIB386)
   stays free of any dependency on CONSOLE (SOURCES). Maps the line's kind and
   severity to a console colour — the in-engine console shows level/structure as
   colour, not a text tag. Banner/section lines get the cyan structural accent
   (matching the terminal sink's bookends); ordinary lines colour by severity. */
static void ConsoleLogLine(const char *line, LogSeverity sev, LogLineKind kind) {
    int colour = CONSOLE_COL_DEFAULT;
    if (kind == LOG_LINE_BANNER || kind == LOG_LINE_SECTION) {
        colour = CONSOLE_COL_BANNER;
    } else {
        switch (sev) {
        case LOG_DEBUG:
            colour = CONSOLE_COL_DEBUG;
            break;
        case LOG_WARN:
            colour = CONSOLE_COL_WARN;
            break;
        case LOG_ERROR:
            colour = CONSOLE_COL_ERROR;
            break;
        case LOG_INFO:
            colour = CONSOLE_COL_DEFAULT;
            break;
        }
    }
    Console_PrintColored(colour, "%s", line);
}

void InitAdeline(S32 argc, char *argv[]) {
    {
        char resFolderPath[ADELINE_MAX_PATH] = "";
        char saveFolderPath[ADELINE_MAX_PATH] = "";
        char cfgFilePath[ADELINE_MAX_PATH] = "";
        char logFilePath[ADELINE_MAX_PATH] = "";

        GetResPath(resFolderPath, ADELINE_MAX_PATH, NULL);
        GetSavePath(saveFolderPath, ADELINE_MAX_PATH, NULL);
        GetCfgPath(cfgFilePath, ADELINE_MAX_PATH, CFG_NAME);

        GetLogPath(logFilePath, ADELINE_MAX_PATH, LOG_NAME);
        CreateLog(logFilePath);

        /* Structured boot log (docs/BOOT_LOG_PLAN.md). The file sink reuses
           adeline.log — CreateLog above truncated it for this launch, the sink
           appends; both it and the legacy LogPrintf sites share the one file.
           The terminal sink colours stderr when launched from a real TTY. The
           console sink mirrors records into the in-engine F12 overlay. */
        Log_Init();
        Log_AddSink(Log_MakeFileSink(logFilePath, LOG_DEBUG));
        Log_AddSink(Log_MakeTerminalSink(LOG_DEBUG));
        Log_AddSink(Log_MakeConsoleBufferSink(ConsoleLogLine));
        atexit(Log_Shutdown);

        /* Boot identity + key paths up top, so a pasted log is self-describing. */
        Log_Banner("%s · %s %s · %d cores · %d GB RAM", APPNAME, LOG_PLATFORM_NAME,
                   LOG_ARCH_NAME, SDL_GetNumLogicalCPUCores(),
                   (SDL_GetSystemRAM() + 512) / 1024);
        Log_Raw("Built %s %s", __DATE__, __TIME__);
        Log_Raw("Assets: %s", resFolderPath);
        Log_Raw("Saves:  %s", saveFolderPath);
        Log_Raw("Config: %s", cfgFilePath);
        Log_Raw("Log:    %s", logFilePath);
        Log_Raw("");
    }

    // ··········································································
    {
        if (!InitEvents()) {
            BootFatal("The input/event system could not be initialized.");
        }
        Log_Info("Events     ok");
    }

    // ··········································································
    {
        InitJoystick();
        Log_Info("Joystick   %s", JoystickGetName());
    }

    // --- WINDOW ----------------------------------------------------------------
    /* Window/Render status lines are emitted after InitGraphics below, once the
       surface dimensions and mode are known. */
    {
        if (!InitWindow(APPNAME)) {
            BootFatal("The game window could not be created.");
        }
    }

    // ··········································································
    //  Config File
    //  First run (or the user deleted it): install a default lba2.cfg — copy the
    //  one shipped with the game data, or fall back to the built-in template.
    //  Logged once, after the action, so the boot log states what actually
    //  happened: Info for the copy, Warn for the no-default-in-game-data fallback.
    GetCfgPath(PathConfigFile, ADELINE_MAX_PATH, CFG_NAME);
    if (!ExistsFileOrDir(PathConfigFile)) {
        char PathDefaultConfigFile[ADELINE_MAX_PATH];
        GetDefaultCfgPath(PathDefaultConfigFile, ADELINE_MAX_PATH, CFG_NAME);
        if (ExistsFileOrDir(PathDefaultConfigFile)) {
            if (!Copy(PathDefaultConfigFile, PathConfigFile)) {
                BootFatal("Could not copy the configuration file to '%s'.",
                          PathConfigFile);
            }
            Log_Info("Config     created from game-data default");
        } else {
            if (!WriteEmbeddedDefaultLba2Cfg(PathConfigFile)) {
                BootFatal("Could not write a default configuration file to '%s'.",
                          PathConfigFile);
            }
            Log_Warn("Config     wrote built-in template (no default in game data)");
        }
    }

    // ··········································································
    //  CMDLINE
    GetCmdLine(argc, argv);

    // OS/platform is reported in the boot header (LOG_PLATFORM_NAME).

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

    if (!InitVideo()) {
        BootFatal("The video system could not be initialized.");
    }

    if (!InitScreen()) {
        BootFatal("The display surface could not be created.");
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
        /* Read the fullscreen choice now so the window is created fullscreen up
           front; the later ReadConfigFile -> SetWindowFullscreen pass then just
           confirms it instead of flipping a windowed window. */
        const bool reqFullscreen = Res_LoadBootFullscreen();
        if (!InitGraphics(reqResX, reqResY, reqFullscreen)) {
            BootFatal("The graphics mode %ux%u could not be set.", reqResX,
                      reqResY);
        }
        /* The "Display" status line is logged from main() after InitProgram,
           alongside the rest of the post-init summary. */
    }

    // ··········································································
    //  Midi + Sample devices

#if ((inits) & INIT_MIDI)
#ifndef LIB_AIL
#error ADELINE: you need to include AIL.H
#endif
    if (!InitMidiDriver(NULL)) {
        BootFatal("The MIDI audio device could not be initialized.");
    }
#endif

    // ··········································································
    //  Sample device
    if (Control_NoAudio()) {
        Log_Info("Audio      disabled (--no-audio)");
    } else {
#ifndef LIB_AIL
#error ADELINE: you need to include AIL.H
#endif
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
            Log_Warn("Audio      none — running silently");
        } else {
            Log_Info("Audio      44100 Hz stereo");
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
