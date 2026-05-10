#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolve the game asset directory (must contain lba2.hqr per IsValidResourceDir).
 * Order: --game-dir/--data-dir, LBA2_GAME_DIR, persisted LastGameDir from a
 * previous picker session, SDL base path candidates, cwd, parent walk,
 * relative dev paths, then sibling scan (parent of cwd: each subdirectory and
 * CommonClassic/common/Common/Classic).
 *
 * Strips --game-dir and --data-dir from argv and updates *argc before GetCmdLine.
 *
 * @return true if outDir is valid; false if no candidate matched (caller may
 *         show the first-launch picker via PromptForResDir, then exit on cancel).
 */
bool ResolveGameDataDir(char *outDir, U16 outMax, int *argc, char **argv);

/**
 * Persist a user-chosen game-data directory to <prefPath>/last_game_dir.txt.
 * Read back on next launch as a high-priority discovery probe in
 * ResolveGameDataDir, before auto-discovery falls through.
 *
 * Writes via direct stdio rather than the lba2.cfg buffer machinery —
 * happens at picker-success time, before InitDirectories/InitAdeline have
 * set up PathConfigFile or the engine's main memory pool. Self-contained.
 *
 * @return true on successful write; false if SDL_GetPrefPath or fopen fails.
 */
bool WritePersistedGameDir(const char *path);

#ifdef __cplusplus
}
#endif
