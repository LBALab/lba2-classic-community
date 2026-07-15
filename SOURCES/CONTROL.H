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

/* --- Harness input timeline (docs/INPUT_SIM_PLAN.md) -----------------------
 *
 * Drives the hero from the console `input` command / --exec with holds and sequences metered in
 * SIM TICKS, not rendered frames. That is the point: a rendered frame the fixed-timestep throttle
 * skips consumes no tick, so a hold means the same amount of gameplay whatever --fixed-timestep is
 * (with the old render-frame count, `input up 30` walked full distance at --fixed-timestep 16 and
 * zero at 100). The current mask is OR'd into Input in MainLoop, so injected input rides the same
 * path as real input. All durations are sim ticks.
 */

/* Clear the timeline (console `input off`). */
void Control_InputClear(void);

/* Append `mask` held from sim tick `startTick` for `durTicks` ticks. Returns 1 on success, 0 if
   the event table is full. A zero mask or zero duration is a no-op gap. Builds `input seq`. */
int Control_InputSchedule(U32 startTick, U32 durTicks, U32 mask);

/* Like Control_InputSchedule but measured in RENDERED frames, not sim ticks. This is the tool for
   reproducing the throttle input-EDGE drop: a press placed on a throttle-skipped frame (a frame
   the sim does not step) is never seen by the object loop, exactly as a real high-refresh display
   drops it. Builds `input fseq`. */
int Control_InputScheduleFrame(U32 startFrame, U32 durFrames, U32 mask);

/* Replace the timeline with a single hold of `mask` for `durTicks` ticks
   (console `input <flags> [ticks]`). */
void Control_InputHold(U32 mask, U32 durTicks);

/* OR of every event held at the current sim tick. MainLoop ORs this into Input each frame while
   Control_InputPending(). */
U32 Control_InputCurrentMask(void);

/* Non-zero while a scheduled event has not yet elapsed. */
int Control_InputPending(void);

/* Advance the timeline by `simTicks` (the sim steps a frame ran). Call once per sim-running frame
   in MainLoop; skipped frames must not call it. */
void Control_InputAdvance(int simTicks);

/* Advance the rendered-frame clock. Call once per rendered frame (skipped frames included) so a
   frame-based (`fseq`) event lands on the exact frame requested. */
void Control_InputFrameTick(void);

/* Toggle a per-sim-tick trace of the injected mask + hero gen_anim/pos to the debug log
   (console `input trace <0|1>`), so a headless run can watch the hero respond. */
void Control_InputTrace(int on);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_H */
