#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Initialization ----------------------------------------------------------
void InitDirectories(const char *userDir, const char *resDir,
                     const char *cfgDir, const char *CDPrefix, S32 distVersion);
void EndDirectories();

// --- Interface ---------------------------------------------------------------
void GetCurrentRunningDir(char *outDir); ///< Path where application was run
void GetDefaultUserDir(char *outDir);    ///< Path for user writable data
void GetDefaultResDir(char *outDir);     ///< Path for game resources

/// True if \p path is a directory containing lba2.hqr (case-discovered).
bool IsValidResourceDir(const char *path);

/// True if \p path lives under the resolved resource (game-data) directory.
/// Lets cleanup spare installed assets and only remove transient copies.
bool Directories_IsUnderResDir(const char *path);

/// The filename that marks a valid resource directory ("lba2.hqr"). Used to
/// locate the asset-root subtree inside a mounted disc image.
const char *Directories_GetResMarker(void);

void GetDefaultCfgPath(char *outPath, U16 pathMaxSize, const char *cfgFilename);

void GetCfgPath(char *outPath, U16 pathMaxSize, const char *cfgFilename);
void GetSavePath(char *outPath, U16 pathMaxSize, const char *saveFilename);
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
