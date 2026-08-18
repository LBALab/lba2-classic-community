/* How the program ends, and what it tells the player on the way out.
 *
 * The exit report, the fatal-error dialog, the two LBA_NORETURN exits and the
 * boot-time failure path. Together they are what a player sees when the game
 * cannot start, which is a concern of its own and not part of the main loop that
 * PERSO.CPP holds.
 *
 * A surface in CODESTYLE's sense: everything calls it and it owns no state of
 * anyone else's, so nothing here reaches for a subsystem's variables. The two
 * globals it does write, End_Num and End_Error, are the report itself, and they
 * stay on the shared bus because the callers that set them are spread across the
 * engine.
 *
 * The decision half is in BOOT_END.H, free of SDL and the log, so a host test
 * can ask what a failing start says without one happening.
 */
#include "C_EXTERN.H"

#include "BOOT_END.H"
#include "BOOT_EXIT.H"
#include "CONFIG_FILE.H"
#include "CONTROL.H"
#include "DIRECTORIES.H"
#include "EXIT_SCREEN.H"

#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H>

#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

/* The report itself, and private to this file. Every caller reaches it through
   TheEnd, InitTheEnd or TheEndCheckFile below rather than assigning it, which is
   what lets the exit path be the only thing that decides what an exit means.

   -1 until something sets it: an early exit has neither failed nor succeeded,
   and BootEnd_IsFailure treats it as neither. */
static S32 End_Num = -1;
static const char *End_Error;

void BootExit_NoteContext(const char *what) {
    End_Error = what;
}

void ErrorHQRGet(char *name, S32 index) {
    char errstring[256];

    snprintf(errstring, sizeof(errstring), "HQR_Get(%s): buffer trop petit (index %d)", name, index);

    TheEnd(NOT_ENOUGH_MEM, errstring);
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
LBA_NORETURN void TheEndCheckFile(const char *filename) {
    if (FileSize(filename))
        End_Num = NOT_ENOUGH_MEM;
    else
        End_Num = ERROR_NOT_FOUND_FILE;

    End_Error = filename;

    /* Always an error path -> non-zero so launchers/scripts/Steam can tell.
       (The visible message for GUI launches is shown by TheEndInfo.) */
    exit(EXIT_FAILURE);
}

/*══════════════════════════════════════════════════════════════════════════*/
void InitTheEnd(S32 num, char *error) {
    End_Num = num;
    End_Error = error;
}

/*══════════════════════════════════════════════════════════════════════════*/
void TheEnd(S32 num, const char *error) {
    End_Num = num;
    End_Error = error;

    /* Clean exit only for PROGRAM_OK; every other End_Num is a failure. */
    exit(num == PROGRAM_OK ? 0 : EXIT_FAILURE);
}

/*══════════════════════════════════════════════════════════════════════════*/
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
void DebugHQR(const char *titre, T_HQR_HEADER *hqr) {
    /* One call per line: titre is a %s argument (not the format), so it is
       safe and stays on the same line as its stats under the line-oriented
       log fan-out (see docs/LOGGING_UNIFICATION.md). */
    if (hqr) {
        LogPrintf("%s: %7ld - used: %7ld - MaxIndex: %4ld - NbIndex: %4ld\n", titre, hqr->MaxSize, hqr->MaxSize - hqr->FreeSize, hqr->MaxIndex, hqr->NbIndex);
    } else {
        LogPrintf("%s: xxxxxxx - used: xxxxxxx - MaxIndex: xxxx - NbIndex: xxxx\n", titre);
    }
}
#endif

/*══════════════════════════════════════════════════════════════════════════*/

// Trouver le moyen de cleaner cette @#&? de fonction !!!!!!

void V_ColorLine(U8 text, U8 back, U16 ncars) {
    text = text;
    back = back;
    ncars = ncars;
}

/*══════════════════════════════════════════════════════════════════════════*/
/* Native fatal-error dialog for terminal-less launches (double-clicked
   .app/.exe, Steam Deck), where the log/terminal is invisible. No-op on a dev
   terminal run (the log is right there), under the control harness (never block
   automation), or when no display is available (headless -> log only). Mirrors
   the no-game-data dialog in RES_PICKER. */
static void ShowFatalErrorDialog(const char *title, const char *message) {
    if (isatty(fileno(stderr)))
        return;
    if (Control_IsActive())
        return;
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO))
        return;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
}

/*══════════════════════════════════════════════════════════════════════════*/
/* Fatal exit for InitAdeline init failures -- see PERSO.H for why these can't
   go through TheEnd/TheEndInfo. Logs the reason, surfaces the native dialog on
   terminal-less launches, exits non-zero. No teardown: the failing subsystem
   never finished coming up, so there is nothing safe to tear down. */
LBA_NORETURN void BootFatal(const char *fmt, ...) {
    char reason[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(reason, sizeof reason, fmt, ap);
    va_end(ap);

    Log_Error("%s", reason);

    char logPath[ADELINE_MAX_PATH] = "";
    GetLogPath(logPath, ADELINE_MAX_PATH, LOG_NAME);
    char msg[ADELINE_MAX_PATH + 640];
    snprintf(msg, sizeof msg, "%s\n\nThe game can't start.\nDetails: %s", reason,
             logPath);
    ShowFatalErrorDialog("LBA2: cannot start", msg);

    exit(EXIT_FAILURE);
}

/*══════════════════════════════════════════════════════════════════════════*/
void TheEndInfo() {
    S32 err = FALSE;

#ifdef CDROM
    ClearVoiceFile();
#endif
    StopMusic();

    /* No version/build footer here — the boot header already stamps version,
       build, platform, and backends at the top of every log. */

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
    LogPrintf("* Start Extended Memory was %ld\n", MemoMemory);
    LogPrintf("* Start Dos Memory was %ld\n", MemoDosMemory);
    LogPrintf("* Min Dos Memory was %ld\n", MemoMinDosMemory);
    LogPrintf("* Size PtrZBuffer: %ld\n", ListMem[MEM_PTR_ZBUFFER].Size);

    DebugHQR("* HQR Sprite     ", HQRPtrSprite);
    DebugHQR("      Sprite raw ", HQRPtrSpriteRaw);
    DebugHQR("    Anim 3DS     ", HQRPtrAnim3DS);
    DebugHQR("      Sample     ", HQR_Samples);
    DebugHQR("        Anim     ", HQR_Anims);
    DebugHQR("        Body     ", HQR_Bodys);
    DebugHQR("      ObjFix     ", HQR_ObjFix);
    DebugHQR("  HQR_Isle_Obj   ", HQR_Isle_Obj);
    DebugHQR("  HQR_CubeInfos  ", HQR_CubeInfos);
    DebugHQR("  HQR_ListDecors ", HQR_ListDecors);
    DebugHQR("  HQR_MapPGround ", HQR_MapPGround);
    DebugHQR("  HQR_ListTexDef ", HQR_ListTexDef);
    DebugHQR("  HQR_MapSommetY ", HQR_MapSommetY);
    DebugHQR("  HQR_MapIntens  ", HQR_MapIntensity);

    LogPrintf("* End Extended Memory was %ld\n", MemEnd);
    LogPrintf("* Total Extended Memory used: %ld\n", MemoMemory - MemEnd);

    LogPrintf("\nNbTexEffects= %d\n", NbTexEffects);
    if (PtrPolySea)
        LogPrintf("Animated Sea Enable.\n");
    else
        LogPrintf("Animated Sea Disable.\n");
#endif

    LogPrintf("\n");

    /* Exit-screen easter egg: only on a genuine clean shutdown. End_Num is
       PROGRAM_OK (2) only when TheEnd(PROGRAM_OK, ...) ran; crash/error paths
       set other codes and early exits leave it at -1, so this never fires on a
       non-clean exit. Separate module — does not touch the log; silent unless
       stdout is a real TTY. See docs/plan/BOOT_LOG_PLAN.md Phase 6. */
    if (End_Num == PROGRAM_OK) {
        PrintExitScreen();
    }

    if (BootEnd_IsFailure(End_Num)) {
        V_ColorLine(14, 0, 130);
        LogPrintf(BootEnd_LogFormat(End_Num), End_Error);
        V_ColorLine(7, 0, 130);
    }

    switch (End_Num) {
    case PROGRAM_OK:
#ifdef DEBUG_TOOLS
        if (NbNbf) {
            LogPrintf("* Frame speed status:\n");
            LogPrintf("  Maximal frame rate occured: %d\n", MaxNbf);
            LogPrintf("  Average frame rate was %d\n", TotalNbf / NbNbf);
            LogPrintf("%s\n", End_Error);
        }
#endif
        if (End_Error && End_Error[0])
            LogPrintf("%s\n", End_Error);
        LogPrintf("OK.\n");
        break;
    }

    /* Make fatal errors visible when there's no terminal to read the log. */
    if (BootEnd_IsFailure(End_Num)) {
        char logPath[ADELINE_MAX_PATH] = "";
        char msg[ADELINE_MAX_PATH + 256];

        GetLogPath(logPath, ADELINE_MAX_PATH, LOG_NAME);
        if (BootEnd_FormatMessage(msg, sizeof msg, End_Num, End_Error, logPath) > 0)
            ShowFatalErrorDialog("LBA2: cannot start", msg);
    }

    WriteConfigFile();

    Clear3dExt();
}
