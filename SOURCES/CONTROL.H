#ifndef CONTROL_H
#define CONTROL_H

/*
 * CLI control harness: drive the engine from outside for automation / regression.
 *
 *   lba2 --load <slot> --exec "<cmd>;<cmd>" --tick <N> --fixed-dt <ms> --demo \
 *        --dump-state <path> --screenshot <path> --exit
 *
 * The harness reuses existing engine seams rather than adding new ones: the console
 * command bus (Console_Execute) for --exec, the InitGame(-1) -> ChangeCube() load
 * sequence for --load, SavePNG for --screenshot, and plain global reads for
 * --dump-state. The only engine intrusions are the three call sites in PERSO.CPP
 * (parse, boot branch, and the one tick hook in MainLoop). See docs/CONTROL.md and
 * docs/AUTOMATION_PLAN.md.
 */

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Parse + strip harness flags from argv. Call early in main(), mirroring
   parse_save_load_test_flag: recognised flags and their values are removed in place
   so downstream positional parsing is unaffected. */
void Control_ParseArgs(int *argc, char *argv[]);

/* Non-zero if any harness flag was supplied this run. */
int Control_IsActive(void);
int Control_IsHeadless(void);

/* Non-zero if --verbose was supplied: log scene/hero progress and modal entries to stderr,
   so a headless run is self-explaining and a hang shows what blocked it. */
int Control_IsVerbose(void);

/* Log verbosity via --log-level <debug|info|warn|error>. Control_HasLogLevel is
   non-zero if the flag was supplied; Control_GetLogLevel returns the LogSeverity
   value (as int) to hand to Log_SetLevel. Applied in PERSO.CPP right after
   Log_Init; when absent the engine falls back to LBA2_LOG_LEVEL, then INFO. */
int Control_HasLogLevel(void);
int Control_GetLogLevel(void);

/* Resolution override via --resolution WxH. Returns 1 if the flag was supplied (use
   Control_GetResolutionX/Y), 0 otherwise (use the compile-time RESOLUTION_X/RESOLUTION_Y
   defaults). Parsed in Control_ParseArgs; safe to call after that. */
int Control_HasResolution(void);
long Control_GetResolutionX(void);
long Control_GetResolutionY(void);

/* Language override via --language <name>. Returns 1 if the flag was supplied (use
   Control_GetLanguage as a 0..5 index into MESSAGE.CPP's TabLanguage[]), 0 otherwise
   (the engine reads the cfg's Language key as normal). Accepts full names
   (English / Français / Deutsch / Español / Italiano / Portugues, case-insensitive on
   ASCII letters) and 2-character codes (EN / FR / DE / SP / IT / PO). Parsed in
   Control_ParseArgs; applied in PERSO.CPP after InitLanguage() returns. Doesn't
   touch the cfg file. */
int Control_HasLanguage(void);
int Control_GetLanguage(void);

/* --no-audio: skip InitAIL() and InitSampleDriver() at boot. Returns 1 if the flag
   was supplied, 0 otherwise. With audio init skipped, the SDL audio subsystem
   never opens a stream and the dummy driver's nanosleep pacing (~74% of sys time
   in projection_demo on a WSL2 setup) doesn't run. Every audio entry point in
   LIB386/AIL/SDL/{SAMPLE,STREAM,VIDEO_AUDIO_SDL}.CPP already gates on
   Sample_Driver_Enabled or a non-NULL stream, so playback calls become silent
   no-ops. Harness-only intent; players who actually want audio shouldn't pass
   this flag. */
int Control_NoAudio(void);

/* Set up the boot path and arm the tick hook. Call once, instead of MainGameMenu(),
   when active: applies --load (InitGame(-1) + ChangeCube while FlagLoadGame is set)
   or a fresh InitGame(1), then arms Control_TickHook. Returns 1 on success (caller
   should run MainLoop), 0 on failure such as an unresolvable --load (caller should
   skip MainLoop and exit). */
int Control_Begin(void);

/* Single seam invoked at the top of the MainLoop body. No-op (one flag test) until
   armed. Runs the --exec batch on the first tick (scene loaded by then), and after N
   effective ticks finalizes (--dump-state, --screenshot) and, if --exit, exits via
   TheEnd(). */
void Control_TickHook(void);

/* Capture final --dump-state / --screenshot when MainLoop returns naturally (e.g. the
   demo script reaches LM_THE_END before the --tick budget). Call once between MainLoop()
   and TheEnd() on the harness boot path. No-op if the tick-budget finalize already ran,
   the harness is not active, or no capture flag was supplied. */
void Control_End(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_H */
