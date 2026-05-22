#ifndef CONTROL_H
#define CONTROL_H

/*
 * CLI control harness: drive the engine from outside for automation / regression.
 *
 *   lba2 --load <slot> --exec "<cmd>;<cmd>" --tick <N> \
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

/* Set up the boot path and arm the tick hook. Call once, instead of MainGameMenu(),
   when active: applies --load (InitGame(-1) + ChangeCube while FlagLoadGame is set)
   or a fresh InitGame(1), then arms Control_TickHook. The caller then runs MainLoop. */
void Control_Begin(void);

/* Single seam invoked at the top of the MainLoop body. No-op (one flag test) until
   armed. Runs the --exec batch on the first tick (scene loaded by then), and after N
   effective ticks finalizes (--dump-state, --screenshot) and, if --exit, exits via
   TheEnd(). */
void Control_TickHook(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_H */
