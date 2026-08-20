#include "INITADEL.H"

#include "C_EXTERN.H"
#include "DIRECTORIES.H"
#include "PERSO.H"      /* BootFatal — fatal exit for init failures */
#include "RES_SWITCH.H" /* Res_LoadBootDimensions — CLI > cfg > default */

#include "AIL/COMMON.H"
#include "CONSOLE/CONSOLE.H" /* console buffer sink printer adapter */
#include "CONTROL.H"
#include "JOYSTICK.H"
#include "RES_DISCOVERY.H" /* Res_GetDiscoverySource for the Assets banner line */
#include <SYSTEM/LOG.H>
#include "SVGA/INITMODE.H"
#include "SVGA/SCREEN.H"
#include "SVGA/VIDEO.H"
#include "SYSTEM/CMDLINE.H"
#include "SYSTEM/CPU.H"
#include "SYSTEM/DEFFILE.H"
#include "SYSTEM/DISCIMG.H"
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
#include "BOOT_EXIT.H"
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
        char recFolderPath[ADELINE_MAX_PATH] = "";
        char cfgFilePath[ADELINE_MAX_PATH] = "";
        char logFilePath[ADELINE_MAX_PATH] = "";

        GetResPath(resFolderPath, ADELINE_MAX_PATH, NULL);
        GetSavePath(saveFolderPath, ADELINE_MAX_PATH, NULL);
        GetRecordingsPath(recFolderPath, ADELINE_MAX_PATH, NULL);
        GetCfgPath(cfgFilePath, ADELINE_MAX_PATH, CFG_NAME);
        GetLogPath(logFilePath, ADELINE_MAX_PATH, LOG_NAME);

        /* The structured log + file/terminal sinks are already up: main() starts
           them before game-data discovery so the discovery/picker diagnostics are
           captured (docs/plan/BOOT_LOG_PLAN.md). Add the in-engine F12 console sink now
           — it has no surface until we're past early boot, so it joins here rather
           than at Log_Init. The file sink reuses adeline.log (CreateLog truncated
           it for this launch); both it and the legacy LogPrintf sites share it. */
        Log_AddSink(Log_MakeConsoleBufferSink(ConsoleLogLine));

        /* Boot identity + key paths up top, so a pasted log is self-describing. */
        Log_Banner("%s · %s %s · %d cores · %d GB RAM", APPNAME, LOG_PLATFORM_NAME,
                   LOG_ARCH_NAME, SDL_GetNumLogicalCPUCores(),
                   (SDL_GetSystemRAM() + 512) / 1024);
        Log_Raw("Built %s %s", __DATE__, __TIME__);
        /* Which probe found the assets, in parentheses after the path. The probe
           list runs silently, so "the engine booted the wrong install" and "the
           engine ignored what I set" look identical in a bug report. Naming the
           winner makes a pasted banner enough to tell them apart. */
        {
            const char *foundVia = Res_GetDiscoverySource();
            if (foundVia != NULL && foundVia[0] != '\0') {
                Log_Raw("Assets: %s  (%s)", resFolderPath, foundVia);
            } else {
                Log_Raw("Assets: %s", resFolderPath);
            }
        }
        /* When a disc image is mounted, one aligned line beside Assets: naming
           the image (basename only; it lives in the assets dir above). Silent
           (no line) otherwise, so the banner is byte-identical to a
           filesystem-only install. */
        {
            char discPath[ADELINE_MAX_PATH] = "";
            S32 discFiles = 0;
            if (DiscImage_GetBannerInfo(discPath, ADELINE_MAX_PATH, &discFiles)) {
                const char *discName = strrchr(discPath, '/');
#ifdef _WIN32
                /* Windows paths may use '\\'; keep whichever separator is last. */
                const char *bs = strrchr(discPath, '\\');
                if (bs != NULL && (discName == NULL || bs > discName))
                    discName = bs;
#endif
                discName = discName ? discName + 1 : discPath;
                Log_Raw("Disc:   mounted %s  (ISO9660, %d files)", discName, (int)discFiles);
            }
        }
        Log_Raw("Saves:  %s", saveFolderPath);
        /* Beside the saves rather than under them, so a player looking for what they
           just recorded does not find it by opening save/. A recording named without a
           directory lands here, which is what `rec` with no argument does, so the one
           place the name has to be resolvable from is a pasted banner. */
        Log_Raw("Recs:   %s", recFolderPath);
        Log_Raw("Config: %s", cfgFilePath);
        Log_Raw("Log:    %s", logFilePath);
        /* Where the three above came from. The Assets line names the probe that
           won for the same reason: a run that wrote somewhere unexpected for a
           good reason (a forgotten LBA2_USER_DIR, a --profile in a shortcut)
           and one that is simply wrong look identical in a bug report
           otherwise. Silent on a plain run, so an install that names neither
           keeps the banner it always had. */
        {
            const char *profile = Directories_GetProfile();
            const char *source = Directories_GetUserDirSource();
            const int named = (profile[0] != '\0');
            const int overridden = (strcmp(source, "default") != 0);
            if (named && overridden) {
                Log_Raw("Writes: profile '%s' under %s", profile, source);
            } else if (named) {
                Log_Raw("Writes: profile '%s'", profile);
            } else if (overridden) {
                Log_Raw("Writes: %s", source);
            }
        }
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
    //  Nothing is copied. The config beside the game data is read as a layer
    //  under this run's own (see the DefFile section below), so it keeps
    //  answering for the game data every boot instead of being snapshotted into
    //  a profile that then outlives the install it was taken from.
    //
    //  An install shipping no config has nothing to layer, so the built-in
    //  template is installed once, exactly as before. It is a seed and not a
    //  third layer on purpose: as a layer its values would also reach every
    //  install that ships a config but leaves a key out, changing settled
    //  defaults (FullScreen among them) on machines that never saw it.
    GetCfgPath(PathConfigFile, ADELINE_MAX_PATH, CFG_NAME);
    {
        char PathSeedConfigFile[ADELINE_MAX_PATH];
        GetDefaultCfgPath(PathSeedConfigFile, ADELINE_MAX_PATH, CFG_NAME);
        if (!ExistsFileOrDir(PathConfigFile) && !ExistsFileOrDir(PathSeedConfigFile)) {
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
            Log_Warn("Audio      none - running silently");
        } else if (!Sample_DriverPlaysSound()) {
            /* The null backend initialises and reports success, then plays nothing.
               Saying "44100 Hz stereo" there sends anyone debugging silence, or a
               replay that diverges on audio, to look at the device. */
            Log_Info("Audio      none - built without a sound backend");
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
    //
    //  Two layers, highest priority first: the config this run owns, then the
    //  one shipped beside the game data. A key is taken from the first layer
    //  that defines it; a key neither defines falls to the compiled default the
    //  read site names.
    //
    //  This replaced copying the game-data config into a fresh profile. A copy
    //  is a snapshot: whichever install a profile first booted against decided
    //  its release, language and key bindings for good, and pointing that same
    //  profile at another release left it announcing the first one's publisher
    //  while reading the second one's data. Layering keeps the game data's keys
    //  answering for the game data, every boot, while the keys the player has
    //  actually changed stay theirs -- WriteConfigFile writes only those, into
    //  the owned file alone.
    //
    //  Nothing writes through this buffer: DefFileBufferWriteString refuses
    //  while layers are attached, and WriteConfigFile re-loads the owned file
    //  first.
    {
        char PathDataConfigFile[ADELINE_MAX_PATH];
        S32 fromData;

        DefFileBufferInit(PathConfigFile, (void *)(ibuffer), ibuffersize);

        GetDefaultCfgPath(PathDataConfigFile, ADELINE_MAX_PATH, CFG_NAME);
        fromData = DefFileBufferAppendFile(PathDataConfigFile);

        Log_Info("Config     %s%s", ExistsFileOrDir(PathConfigFile) ? "own" : "own (none yet)",
                 fromData ? " + game data" : "");
    }

    // ··········································································
}

// ··········································································
