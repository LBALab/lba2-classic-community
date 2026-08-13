#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

/// Org and app passed to SDL_GetPrefPath, so every user-writable path (saves,
/// lba2.cfg, adeline.log, last_game_dir.txt) agrees on one folder.
///
/// The demo SKU takes its own. Its marker, RESS.HQR, is present in retail
/// installs too, so a demo binary handed a retail path validates it; sharing
/// last_game_dir.txt would hand it exactly that path the moment a retail build
/// ran first. Separate folders keep the two SKUs from pointing each other at
/// data neither can use, and keep demo saves out of a retail profile.
/// The marker that makes a tree self-contained, and the folder it names. An
/// empty file beside the binary is the whole feature; its contents are reserved
/// and nothing parses them. Each SKU takes its own folder for the same reason it
/// takes its own per-user one.
#define ADELINE_PORTABLE_MARKER "portable.txt"
#ifdef DEMO
#define ADELINE_PORTABLE_DIR "User-Demo"
#else
#define ADELINE_PORTABLE_DIR "User"
#endif

#define ADELINE_PREF_ORG "Twinsen"
#ifdef DEMO
#define ADELINE_PREF_APP "LBA2-Demo"
#else
#define ADELINE_PREF_APP "LBA2"
#endif

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Initialization ----------------------------------------------------------
void InitDirectories(const char *userDir, const char *resDir,
                     const char *cfgDir, const char *CDPrefix, S32 distVersion);
void EndDirectories();

/// Point the user directory somewhere other than the per-user default, for the
/// whole run: saves, lba2.cfg, adeline.log and the remembered game-data path
/// all follow it. Precedence is this override, then LBA2_USER_DIR, then the
/// per-user path SDL picks.
///
/// Call before the first GetDefaultUserDir: the resolved root is cached on that
/// first call, and a later override would leave the two halves of a run writing
/// to different folders.
///
/// The directory is created if it does not exist, parents included, so naming a
/// fresh one is enough to get an empty profile. A path that cannot be created
/// is left in place rather than silently swapped for the default; the boot then
/// stops in InitDirectories naming the path that failed.
void Directories_SetUserDirOverride(const char *path);

/// Select a named profile: saves, config, log and the remembered game-data path
/// move into `profiles/<name>/` under the user directory, so several installs
/// can be kept apart without naming a folder for each.
///
/// Composes with the override above, which says where the root is while this
/// says which profile inside it. The default profile is the root itself, so a
/// run that names none writes where it always did.
///
/// Same timing rule as the override: call before the first GetDefaultUserDir.
void Directories_SetProfileOverride(const char *name);

/// The profile in force, or an empty string for the default one.
const char *Directories_GetProfile(void);

/// Which of the three resolved the user directory: "--user-dir",
/// "LBA2_USER_DIR" or "default". For the boot banner, so a pasted log says how
/// the run chose where to write, not just where it wrote.
const char *Directories_GetUserDirSource(void);

/// True when \p name can be a profile directory: not empty, no path separator
/// in either spelling, no parent-directory hop, not a bare dot. A profile lives
/// inside the user directory and has to stay there.
bool Directories_IsValidProfileName(const char *name);

/// The precedence rule behind GetDefaultUserDir, without the caching or the
/// directory creation, so it can be exercised directly: \p overridePath (the
/// command line) beats \p envPath (LBA2_USER_DIR), and an empty string counts
/// as absent.
///
/// Returns false when neither names anything, leaving \p outDir untouched;
/// that is the caller's signal to use the per-user path SDL picks. Otherwise
/// writes the winner with a trailing separator, or verbatim when there is no
/// room left for one.
bool Directories_ResolveUserDir(char *outDir, U16 outMax,
                                const char *overridePath, const char *envPath);

/// The user directory a portable tree writes to, given the folder the binary
/// sits in: `<baseDir>/User/`, or `User-Demo/` for the demo build.
///
/// Returns false when \p baseDir names nothing, when no marker sits beside the
/// binary, or when the composed path will not fit, leaving \p outDir untouched.
/// A directory called portable.txt is not a marker.
///
/// The marker is looked for beside the binary rather than in the working
/// directory: a shortcut, a file manager and a launcher each set a different
/// working directory, and the one thing this file has to be is stable.
bool Directories_PortableUserDir(char *outDir, U16 outMax, const char *baseDir);

// --- Interface ---------------------------------------------------------------
void GetCurrentRunningDir(char *outDir); ///< Path where application was run
void GetDefaultUserDir(char *outDir);    ///< Path for user writable data
void GetDefaultResDir(char *outDir);     ///< Path for game resources

/// True if \p path is a directory containing lba2.hqr (case-discovered).
/// Cheap: a handful of stats. Safe to call for every speculative candidate.
bool IsValidResourceDir(const char *path);

/// As IsValidResourceDir, but also accepts a directory whose game data exists
/// only inside a disc image sitting in it (a retail CD rip is one .bin/.cue).
/// Costs a walk of the directory, opening each file to identify it, so use it
/// only for a path somebody named: an explicit override, or the folder already
/// chosen. Calling it across the speculative candidate sweep is what turns
/// discovery from a few stats into minutes of I/O.
bool IsValidResourceDirOrImage(const char *path);

/// True if \p path lives under the resolved resource (game-data) directory.
/// Lets cleanup spare installed assets and only remove transient copies.
bool Directories_IsUnderResDir(const char *path);

/// The filename that marks a valid resource directory ("lba2.hqr"). Used to
/// locate the asset-root subtree inside a mounted disc image.
const char *Directories_GetResMarker(void);

void GetDefaultCfgPath(char *outPath, U16 pathMaxSize, const char *cfgFilename);

void GetCfgPath(char *outPath, U16 pathMaxSize, const char *cfgFilename);
void GetSavePath(char *outPath, U16 pathMaxSize, const char *saveFilename);

/// Copy any saves the install ships into this profile's save folder, once,
/// the first time the profile is used. A retail install ships none and this
/// does nothing; the 1997 demo ships the three its fixed slots load.
/// Call before anything creates the save folder.
void Directories_SeedSavesFromGameData(void);
void GetShootPath(char *outPath, U16 pathMaxSize, const char *shootFilename);
void GetBugPath(char *outPath, U16 pathMaxSize, const char *bugFilename);
void GetJinglePath(char *outPath, U16 pathMaxSize, const char *jingleFilename);
void GetVoicePath(char *outPath, U16 pathMaxSize, const char *voiceFilename,
                  bool fromCD = false);
void GetMoviePath(char *outPath, U16 pathMaxSize, const char *movieFilename);
void GetResPath(char *outPath, U16 pathMaxSize, const char *resFilename);
void GetLogPath(char *outPath, U16 pathMaxSize, const char *logFilename);
/// Like GetLogPath but takes the user dir explicitly, so it works before
/// InitDirectories (the boot log starts before game-data discovery resolves).
void GetLogPathInDir(char *outPath, U16 pathMaxSize, const char *userDir,
                     const char *logFilename);

// =============================================================================
#ifdef __cplusplus
}
#endif
