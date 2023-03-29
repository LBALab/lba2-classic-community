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

void GetCfgPath(char *outPath, U16 pathMaxSize, const char *cfgFilename);
void GetSavePath(char *outPath, U16 pathMaxSize, const char *saveFilename);
void GetShootPath(char *outPath, U16 pathMaxSize, const char *shootFilename);
void GetJinglePath(char *outPath, U16 pathMaxSize, const char *jingleFilename);
void GetVoicePath(char *outPath, U16 pathMaxSize, const char *voiceFilename,
                  bool fromCD = false);
void GetMoviePath(char *outPath, U16 pathMaxSize, const char *movieFilename);
void GetResPath(char *outPath, U16 pathMaxSize, const char *resFilename);
void GetLogPath(char *outPath, U16 pathMaxSize, const char *logFilename);

// =============================================================================
#ifdef __cplusplus
}
#endif
