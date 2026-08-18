#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include <SYSTEM/ADELINE_TYPES.H>

#include "SETTINGS.H"

/* lba2.cfg: reading the player's settings at boot and writing them back.
 *
 * Unlike a subsystem module, this file owns almost none of what it touches: its
 * job is to serialise other people's state, so nearly every value it assigns is
 * declared in someone else's header. The three statics it does own are the
 * per-run override bookkeeping, which exists only for WriteConfigFile's benefit.
 *
 * Not to be confused with CONFIG.CPP, which is the in-game key-binding menu. */

/* Boot: parse the cfg and apply every key to its live global. */
extern void ReadConfigFile(void);

/* Persist the current settings. Also called mid-run by the options menu and by
 * a resolution switch, so it rewrites rather than appends. Values a per-run
 * override is currently forcing are written back at their stored preference. */
extern void WriteConfigFile(void);

/* Persist one key without rewriting the rest. */
extern S32 WriteConfigValue(const char *ident, S32 value);

/* The language the cfg resolved to, recorded by boot after InitLanguage and
 * before --language may replace it for this run. WriteConfigFile needs it to put
 * the player's own language back rather than persisting the override. */
extern void ConfigFile_NoteBootLanguage(S32 language);

/* The console cvars these settings own, registered from Console_RegisterAll.
 * Their ranges and rules travel with them, so the console cannot set a value the
 * config file would reject. */
extern void ConfigFile_RegisterCvars(void);

/* The declaration for a boot key, by its lba2.cfg name, or NULL for a key this
 * file does not own. For anything that has to state a setting's range or default
 * outside the table: ask, rather than writing the numbers down in a second place
 * the table cannot keep in step. */
extern const T_SETTING *ConfigFile_Setting(const char *key);

/* Tell the settings table that this run set VSync, for the console's `vsync`
   command, which takes on/off/toggle and so assigns the value itself rather than
   going through the cvar path. Without it a --vsync run that the player then
   changed by hand would be written back as untouched. */
extern void ConfigFile_MarkVSyncSetThisRun(void);

#endif #endif /* CONFIG_FILE_H */
