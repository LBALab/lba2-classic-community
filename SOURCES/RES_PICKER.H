#ifndef LBA2_RES_PICKER_H
#define LBA2_RES_PICKER_H

/* First-launch folder-picker fallback for game-data discovery.
 *
 * Called by main() when the existing RES_DISCOVERY probe chain (CLI flag,
 * LBA2_GAME_DIR env, persisted LastGameDir, auto-discovery) all fail in a
 * windowed environment. Shows the platform-native folder dialog via
 * SDL_ShowOpenFolderDialog, validates the chosen folder against
 * IsValidResourceDir (lba2.hqr presence), re-prompts on invalid pick,
 * returns the chosen path on success.
 *
 * Headless detection: returns false if SDL_INIT_VIDEO can't be brought
 * up. Caller falls through to the existing "no game data" exit path
 * with the same log message it always produced.
 *
 * See docs/GAME_DATA.md for the full discovery precedence and UX. */

#include "SYSTEM/ADELINE_TYPES.H"

#ifdef __cplusplus
extern "C" {
#endif

/* Fills outPath with a user-chosen valid game-data directory.
 * Returns true on a successful pick (validated by IsValidResourceDir).
 * Returns false on cancel, error, or headless environment.
 *
 * Blocks until the user dismisses the dialog (callback-driven async API
 * adapted to a synchronous loop via SDL_PumpEvents). */
bool PromptForResDir(char *outPath, U16 outMax);

#ifdef __cplusplus
}
#endif

#endif
