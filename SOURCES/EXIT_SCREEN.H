#ifndef EXIT_SCREEN_H
#define EXIT_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exit-screen easter egg: on a clean shutdown, prints a random quote to stdout,
 * and only when stdout is a real terminal (silent when redirected/piped, so it
 * never leaks into logs or the control harness). Standalone module — no
 * dependency on the logging subsystem. Called from TheEndInfo() guarded by
 * End_Num == PROGRAM_OK; not an atexit handler, never on crash/error exits.
 * See docs/BOOT_LOG_PLAN.md Phase 6. */
void PrintExitScreen(void);

#ifdef __cplusplus
}
#endif

#endif /* EXIT_SCREEN_H */
