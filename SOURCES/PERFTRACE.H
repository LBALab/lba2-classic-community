#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Lightweight per-frame timing capture for diagnosing perf cliffs that need
   real user input or scene state to reproduce (high-resolution stalls, slow
   modal loops, platform-specific frame-pacing). Ring buffer in memory
   (PERFTRACE_RING_SIZE samples); zero cost when inactive, opt-in via the
   `perftrace` console verb. See docs/PERFTRACE.md. */

enum {
    PERFTRACE_PHASE_SCENE = 0,
    PERFTRACE_PHASE_PRESENT = 1,
    PERFTRACE_PHASE_COUNT = 2
};

void Perftrace_Init(void);

/* Per-frame begin/end. Call from the main loop body, around one AffScene() +
   UnlockVideoSurface() pair. Inert until Perftrace_Start() is called. */
void Perftrace_FrameBegin(void);
void Perftrace_FrameEnd(void);

/* Phase wrappers. Nest inside FrameBegin/FrameEnd. */
void Perftrace_PhaseBegin(int phase);
void Perftrace_PhaseEnd(int phase);

/* No-arg trampolines matching the SDL PresentTimingFn signature so the
   present-side hook can be registered with SetPresentTimingCallbacks(). */
void Perftrace_PresentBegin(void);
void Perftrace_PresentEnd(void);

/* Capture control. */
void Perftrace_Start(void);
void Perftrace_Stop(void);
int Perftrace_IsActive(void);
void Perftrace_Reset(void);

/* Snapshot the last `max_samples` frames into a text file. Returns 0 on
   success, -1 on file-open failure. */
int Perftrace_Dump(const char *path, int max_samples);

/* Arm a dump that fires automatically on process exit (via atexit). Useful
   for headless harness runs where the user can't interactively type
   `perftrace dump` between the tick loop and exit. NULL or empty disarms. */
void Perftrace_SetAutoDumpPath(const char *path);

/* Short status line to a caller-provided sink (the console, typically).
   Avoids a Console_Print direct dependency. */
typedef void (*Perftrace_PrintFn)(const char *line);
void Perftrace_PrintStatus(Perftrace_PrintFn sink);

#ifdef __cplusplus
}
#endif
