#include "DIRECTORIES.H"

#include "DEFINES.H"

#include <SYSTEM/FILES.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/LOGPRINT.H>

#include <SDL2/SDL_filesystem.h>
#include <SDL2/SDL_stdinc.h>

#include <assert.h>
#include <string.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define DIR_NAME_SAVE "save"ADELINE_PATH_SEP
#define DIR_NAME_SHOOT "shoot"ADELINE_PATH_SEP
#define DIR_NAME_JINGLE "music"ADELINE_PATH_SEP
#define DIR_NAME_MOVIE "video"ADELINE_PATH_SEP
#define DIR_NAME_VOICE_HD "vox"ADELINE_PATH_SEP
#define DIR_NAME_VOICE_CD "lba2"ADELINE_PATH_SEP"vox"ADELINE_PATH_SEP
#define DIR_NAME_VOICE_CD_US "twinsen"ADELINE_PATH_SEP"vox"ADELINE_PATH_SEP
#define FILE_VALID_RES_DIR "lba2.hqr"

// --- Private state -----------------------------------------------------------
bool directoriesInitialized = false;
char directoriesUserDir[ADELINE_MAX_PATH] = "";
char directoriesResDir[ADELINE_MAX_PATH] = "";
char directoriesCfgDir[ADELINE_MAX_PATH] = "";
char directoriesCDPrefix[ADELINE_MAX_PATH] = "";
S32 directoriesDistVersion;

// --- Private functions -------------------------------------------------------
/**
 * Check if a directory or file formed by \p baseDir + \p checkPath exists
 * taking into consideration multiple variations of lower and upper case
 * for \p checkPath.
 * @param outFound Output for found variation of \p checkPath, if any.
 * @param foundMaxSize Max size of \p outFound string, including null
 *        terminator.
 * @param baseDir Part of path that don't change, wont be tested for
 * variations. Must include ending '\'.
 * @param checkPath Ending part of path that will be tested for multiple
 *        case variations.
 * @return TRUE if discovered. When FALSE \p outFound is unchanged.
 * @note If \p foundMaxSize is 0, \p outFound will be ignored.
 */
bool Discover(char *outFound, U16 foundMaxSize, const char *baseDir,
              const char *checkPath) {
  assert(((foundMaxSize > 0) && (outFound != NULL)) || (foundMaxSize == 0));
  assert(baseDir != NULL);
  assert(checkPath != NULL);

  bool found;
  char fullPath[ADELINE_MAX_PATH];
  char variation[ADELINE_MAX_PATH];

  strncpy(variation, checkPath, ADELINE_MAX_PATH);
  variation[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee

  // Try as is
  snprintf(fullPath, ADELINE_MAX_PATH, "%s%s", baseDir, variation);
  found = ExistsFileOrDir(fullPath);

  if (!found) {
    // Try uppercase
    SDL_strupr(variation);
    snprintf(fullPath, ADELINE_MAX_PATH, "%s%s", baseDir, variation);
    found = ExistsFileOrDir(fullPath);

    if (!found) {
      // Try lowercase
      SDL_strlwr(variation);
      snprintf(fullPath, ADELINE_MAX_PATH, "%s%s", baseDir, variation);
      found = ExistsFileOrDir(fullPath);
    }

    // TODO: Implement try with first letter uppercase (eg: "Music")
  }

  // Copy to out if requested and variation was found
  const bool outRequested = (foundMaxSize > 0);
  if (outRequested && found && (outFound != NULL)) {
    strncpy(outFound, variation, foundMaxSize);
    outFound[foundMaxSize - 1] = '\0'; // Guarantee
  }

  return found;
}

/**
 * Discover correct case for an ending path and append it to a base path.
 * @param outPath Output of operation.
 * @param pathMaxSize Max size of \p outPath string, including null
 *        terminator.
 * @param baseDir Directory path that contains file to be discovered and
 *        composes \p outPath.  Must include ending '\'.
 * @param endingPath Name of directory of file contained in \p baseDir to be
 *        discovered. If no case variation is found, \p outPath will be
 *        composed using unmodified \p baseDir + \p endingPath.
 * @note If \p endingPath is empty or NULL, no discover will occur
 *       and \p outPath will contain only \p baseDir.
 * @see DiscoverFile()
 */
void DiscoverEndingAndAppend(char *outPath, U16 pathMaxSize,
                             const char *baseDir, const char *endingPath) {
  assert((outPath != NULL) && (pathMaxSize > 0));
  assert(baseDir != NULL);

  if ((endingPath != NULL) && (strlen(endingPath) > 0)) {
    char discFile[ADELINE_MAX_PATH];
    bool found = Discover(discFile, ADELINE_MAX_PATH, baseDir, endingPath);

    snprintf(outPath, pathMaxSize, "%s%s", baseDir,
             (found ? discFile : endingPath));
  } else {
    snprintf(outPath, pathMaxSize, "%s", baseDir);
  }
}

bool IsValidResourceDir(const char *path) {
  if (!ExistsFileOrDir(path)) {
    return false;
  }

  // Check if path contain required file
  return Discover(NULL, 0, path, FILE_VALID_RES_DIR);
}

// --- Initialization ----------------------------------------------------------
void InitDirectories(const char *userDir, const char *resDir,
                     const char *cfgDir, const char *CDPrefix,
                     S32 distVersion) {
  assert(directoriesInitialized == false);
  assert(userDir != NULL);
  assert(resDir != NULL);
  assert(cfgDir != NULL);
  assert(CDPrefix != NULL);
  assert(distVersion >= 0);

  strncpy(directoriesUserDir, userDir, ADELINE_MAX_PATH);
  directoriesUserDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
  if (!ExistsFileOrDir(directoriesUserDir)) {
    LogPrintf("Error: Invalid user directory '%s'\n", directoriesUserDir);
    exit(EXIT_FAILURE);
  }

  strncpy(directoriesResDir, resDir, ADELINE_MAX_PATH);
  directoriesResDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
  if (!IsValidResourceDir(directoriesResDir)) {
    LogPrintf("Error: Invalid resource directory '%s'\n", directoriesResDir);
    exit(EXIT_FAILURE);
  }

  strncpy(directoriesCfgDir, cfgDir, ADELINE_MAX_PATH);
  directoriesCfgDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
  if (!ExistsFileOrDir(directoriesCfgDir)) {
    LogPrintf("Error: Invalid config directory '%s'\n", directoriesCfgDir);
    exit(EXIT_FAILURE);
  }

  strncpy(directoriesCDPrefix, CDPrefix, ADELINE_MAX_PATH);
  directoriesCDPrefix[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
  if (strlen(directoriesCDPrefix) > 0) {
    if (!ExistsFileOrDir(directoriesCDPrefix)) {
      LogPrintf("Error: Invalid CD drive or directory '%s'\n",
                directoriesCDPrefix);
      exit(EXIT_FAILURE);
    }
  }

  directoriesDistVersion = distVersion;

  directoriesInitialized = true;
}

void EndDirectories() { directoriesInitialized = false; }

// --- Interface ---------------------------------------------------------------
void GetCurrentRunningDir(char *outDir) {
  static bool initialized = false;
  static char runDir[ADELINE_MAX_PATH] = "";

  if (!initialized) {
    initialized = true;

    char *sdlBasePath = SDL_GetBasePath();
    strncpy(runDir, sdlBasePath, ADELINE_MAX_PATH);
    runDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee

    SDL_free(sdlBasePath);
  }

  strncpy(outDir, runDir, ADELINE_MAX_PATH);
}

void GetDefaultUserDir(char *outDir) {
  static bool initialized = false;
  static char userDir[ADELINE_MAX_PATH] = "";
  if (!initialized) {
    char *sdlUserPath = SDL_GetPrefPath("Twinsen", "LBA2");
    strncpy(userDir, sdlUserPath, ADELINE_MAX_PATH);
    userDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
    SDL_free(sdlUserPath);

    initialized = true;
  }

  strncpy(outDir, userDir, ADELINE_MAX_PATH);
}

void GetDefaultResDir(char *outDir) { GetCurrentRunningDir(outDir); }

void GetCfgPath(char *outPath, U16 pathMaxSize, const char *cfgFilename) {
  assert(directoriesInitialized);
  assert((outPath != NULL) && (pathMaxSize > 0));

  DiscoverEndingAndAppend(outPath, pathMaxSize, directoriesCfgDir, cfgFilename);
}

void GetSavePath(char *outPath, U16 pathMaxSize, const char *saveFilename) {
  assert(directoriesInitialized);
  assert((outPath != NULL) && (pathMaxSize > 0));

  static bool initialized = false;
  static char savePath[ADELINE_MAX_PATH] = "";
  if (!initialized) {
    DiscoverEndingAndAppend(savePath, ADELINE_MAX_PATH, directoriesUserDir,
                            DIR_NAME_SAVE);
    initialized = true;
  }

  DiscoverEndingAndAppend(outPath, pathMaxSize, savePath, saveFilename);
}

void GetShootPath(char *outPath, U16 pathMaxSize, const char *shootFilename) {
  assert(directoriesInitialized);
  assert((outPath != NULL) && (pathMaxSize > 0));

  static bool initialized = false;
  static char shootPath[ADELINE_MAX_PATH] = "";
  if (!initialized) {
    char saveDir[ADELINE_MAX_PATH] = "";
    GetSavePath(saveDir, ADELINE_MAX_PATH, "");

    DiscoverEndingAndAppend(shootPath, ADELINE_MAX_PATH, saveDir,
                            DIR_NAME_SHOOT);
    initialized = true;
  }

  DiscoverEndingAndAppend(outPath, pathMaxSize, shootPath, shootFilename);
}

void GetJinglePath(char *outPath, U16 pathMaxSize, const char *jingleFilename) {
  assert(directoriesInitialized);
  assert((outPath != NULL) && (pathMaxSize > 0));

  static bool initialized = false;
  static char jinglePath[ADELINE_MAX_PATH] = "";
  if (!initialized) {
    DiscoverEndingAndAppend(jinglePath, ADELINE_MAX_PATH, directoriesResDir,
                            DIR_NAME_JINGLE);
    initialized = true;
  }

  DiscoverEndingAndAppend(outPath, pathMaxSize, jinglePath, jingleFilename);
}

// TODO: When direct from CD as resource completely implemented, VOX may
//  need to be stored in user folder since it is streamed and cached to disk...
void GetVoicePath(char *outPath, U16 pathMaxSize, const char *voiceFilename,
                  bool fromCD) {
  assert(directoriesInitialized);
  assert((outPath != NULL) && (pathMaxSize > 0));

  static bool initialized = false;
  static char voicePathHD[ADELINE_MAX_PATH] = "";
  static char voicePathCD[ADELINE_MAX_PATH] = "";
  if (!initialized) {
    DiscoverEndingAndAppend(voicePathHD, ADELINE_MAX_PATH, directoriesResDir,
                            DIR_NAME_VOICE_HD);

    if (strlen(directoriesCDPrefix) > 0) {
      DiscoverEndingAndAppend(voicePathCD, ADELINE_MAX_PATH,
                              directoriesCDPrefix,
                              ((directoriesDistVersion == UNKNOWN_VERSION) ||
                               (directoriesDistVersion == EA_VERSION))
                                  ? DIR_NAME_VOICE_CD
                                  : DIR_NAME_VOICE_CD_US);
    } else {
      // If no CD prefix defined, use folder from resources dir
      DiscoverEndingAndAppend(voicePathHD, ADELINE_MAX_PATH, directoriesResDir,
                              DIR_NAME_VOICE_HD);
    }

    initialized = true;
  }

  if (fromCD) {
    DiscoverEndingAndAppend(outPath, pathMaxSize, voicePathCD, voiceFilename);
  } else {
    DiscoverEndingAndAppend(outPath, pathMaxSize, voicePathHD, voiceFilename);
  }
}

void GetMoviePath(char *outPath, U16 pathMaxSize, const char *movieFilename) {
  assert(directoriesInitialized);
  assert((outPath != NULL) && (pathMaxSize > 0));

  static bool initialized = false;
  static char moviePath[ADELINE_MAX_PATH] = "";
  if (!initialized) {
    DiscoverEndingAndAppend(moviePath, ADELINE_MAX_PATH, directoriesResDir,
                            DIR_NAME_MOVIE);
    initialized = true;
  }

  DiscoverEndingAndAppend(outPath, pathMaxSize, moviePath, movieFilename);
}

void GetResPath(char *outPath, U16 pathMaxSize, const char *resFilename) {
  assert(directoriesInitialized);
  assert((outPath != NULL) && (pathMaxSize > 0));

  // TODO: Review cases when LBA_EDITOR is defined. Some places on
  //       the codebase contain:
  /*
   * #ifdef LBA_EDITOR
   * strcpy(FileText, PathRessource);
   * #endif
   */

  DiscoverEndingAndAppend(outPath, pathMaxSize, directoriesResDir, resFilename);
}

void GetLogPath(char *outPath, U16 pathMaxSize, const char *logFilename) {
  assert(directoriesInitialized);
  assert((outPath != NULL) && (pathMaxSize > 0));

  DiscoverEndingAndAppend(outPath, pathMaxSize, directoriesUserDir,
                          logFilename);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
