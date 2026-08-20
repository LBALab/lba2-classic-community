#ifndef LBA2_RECORD_H
#define LBA2_RECORD_H

/* Session recording. Usage and limits in docs/RECORDING.md.
 *
 * Records the polled device state at the tail of UpdateKeyboardState(), indexed by
 * input poll, and replays it back through the same site so every consumer (MainLoop,
 * modals, CheckKey sites, the pad) sees it without knowing.
 *
 * Three values do not pass through TabKeys and are carried alongside: the pad's
 * first-pressed scancode, the raw right-stick axes, and the mouse deltas plus Click.
 */

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Tail of UpdateKeyboardState(): capture or inject this poll's device state. */
void Record_PollHook(void);

/* ManageTime(): capture or supply the clock where the engine actually reads it. */
void Record_ClockHook(U32 *now);

/* Control_TickHook(): one state hash per tick, the recording's own oracle. */
void Record_TickHook(void);

int Record_Start(const char *path);
int Record_Stop(void);
int Record_Play(const char *path);
void Record_CommandHook(const char *line);
/* Arm from the CLI. The recorder starts on its first input poll rather than from
   MainLoop's tick hook, because the menus never reach that hook. */
void Record_ArmRecord(const char *path);
void Record_ArmReplay(const char *path);
unsigned long Record_Squeeze(const char *path);
/* The file the last Record_Start / Record_Play resolved to. A bare name is a name in
   the recordings folder, so this is where it actually went, which is not the string the
   caller passed. Empty before either has run. */
const char *Record_Path(void);
int Record_IsReplaying(void);
int Record_IsRecording(void);

/* Human-readable report. `path` NULL reports the live run's mode; otherwise it reads
   that recording's header and reports it against the live run, naming each mismatch. */
void Record_Report(const char *path);

#ifdef __cplusplus
}
#endif

#endif
