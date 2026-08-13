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

/**
 * Read back what WritePersistedGameDir stored, into \p outPath.
 * @return false when nothing is stored, leaving \p outPath untouched.
 */
bool ReadPersistedGameDir(char *outPath, U16 outMax);

/**
 * Which probe produced the directory ResolveGameDataDir returned, as a short
 * label for the boot banner ("LBA2_GAME_DIR", "sibling scan", ...).
 *
 * The probe list runs silently, so a run that resolves the "wrong" folder for a
 * good reason is indistinguishable from one that is simply broken: a forgotten
 * LBA2_GAME_DIR outranking the picker looks identical to the picker not working.
 * Naming the winning probe next to the path turns that into a one-line
 * diagnosis.
 *
 * Never NULL. Reads "" before discovery has run, and stays valid for the
 * process lifetime (points at static storage).
 */
const char *Res_GetDiscoverySource(void);

/**
 * True when the game folder came from something typed on this run's command
 * line or in its environment, rather than a remembered pick or a probe.
 *
 * A named profile binds to what the user named, so that `--profile gog` alone
 * works next time. It must not bind to a guess: the probes are a fallback, and
 * writing one down would make an arbitrary folder that profile's install for
 * good.
 */
bool Res_ResolvedFromUserInput(void);

/**
 * Record the probe that resolved the directory. Called by ResolveGameDataDir
 * for its own probes; exposed so the caller can label what it resolved outside
 * them, which today is the first-launch picker.
 */
void Res_SetDiscoverySource(const char *label);

#ifdef __cplusplus
}
#endif
