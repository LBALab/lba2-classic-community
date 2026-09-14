#ifndef CRASH_STATE_H
#define CRASH_STATE_H

/* The game state a crash block reports: the scene, the chapter, the behaviour,
 * whether a cutscene or a fade holds the frame, and the hero's position and body.
 * Plain globals the engine already keeps, registered by address with
 * Crash_AddState (SYSTEM/CRASH.H) and read when the block is written; nothing
 * here is updated for the report's sake. */
void CrashState_Register(void);

#endif /* CRASH_STATE_H */
