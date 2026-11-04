#ifndef CHEATCOD_H
#define CHEATCOD_H

extern void GereCheatCode(void);

/* Cheat table introspection for the menu and console. Names are the typed
 * cheat words ("life", "magic", ...). CheatGetToggleState returns 0/1 for the
 * cheats that toggle and -1 for one-shot cheats. */
int CheatGetCount(void);
const char *CheatGetName(int index);
int CheatGetToggleState(int index);

/* Execute cheat by index into the cheat table. Returns 1 if run, 0 otherwise. */
int TryExecuteCheatByIndex(int index);

/* Execute cheat by name (e.g. "life", "magic"). Returns 1 if matched and run, 0 otherwise. */
int TryExecuteCheatByName(const char *name);

#endif // CHEATCOD_H
