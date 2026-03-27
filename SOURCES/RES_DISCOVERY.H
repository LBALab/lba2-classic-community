#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolve the game asset directory (must contain lba2.hqr per IsValidResourceDir).
 * Order: --game-dir/--data-dir, LBA2_GAME_DIR, SDL base path candidates, cwd,
 * parent walk, relative dev paths.
 *
 * Strips --game-dir and --data-dir from argv and updates *argc before GetCmdLine.
 *
 * @return true if outDir is valid; false if no candidate matched (caller may exit).
 */
bool ResolveGameDataDir(char *outDir, U16 outMax, int *argc, char **argv);

#ifdef __cplusplus
}
#endif
