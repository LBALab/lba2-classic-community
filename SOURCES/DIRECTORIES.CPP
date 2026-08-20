#include "DIRECTORIES.H"

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/FILECOPY.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <stdio.h>

#include <dirent.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef UNKNOWN_VERSION
#define UNKNOWN_VERSION 0
#endif
#ifndef EA_VERSION
#define EA_VERSION 3
#endif

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define DIR_NAME_PROFILES "profiles" ADELINE_PATH_SEP
#define DIR_NAME_SAVE "save" ADELINE_PATH_SEP
#define DIR_NAME_RECORDINGS "recordings" ADELINE_PATH_SEP
#define DIR_NAME_SHOOT "shoot" ADELINE_PATH_SEP
#define DIR_NAME_BUGS "bugs" ADELINE_PATH_SEP
#define DIR_NAME_JINGLE "music" ADELINE_PATH_SEP
#define DIR_NAME_MOVIE "video" ADELINE_PATH_SEP
#define DIR_NAME_VOICE_HD "vox" ADELINE_PATH_SEP
#define DIR_NAME_VOICE_CD "lba2" ADELINE_PATH_SEP "vox" ADELINE_PATH_SEP
#define DIR_NAME_VOICE_CD_US "twinsen" ADELINE_PATH_SEP "vox" ADELINE_PATH_SEP
/* Both spellings below are the way their distribution ships the file.
   Discover() probes as-is, upper, lower and title case, so the set of names
   accepted is the same either way -- but the as-is probe is the one that hits
   on a real install, and the constant is what the "we could not find your game
   data" messages quote back at the player. Naming a spelling nobody has sends
   them off renaming files. */
#ifdef DEMO
/* No demo install ships LBA2.HQR: that bank holds the credits reel, which the
   demo does not have. RESS.HQR is the first bank the demo loads and is present
   in every demo install. */
#define FILE_VALID_RES_DIR "RESS.HQR"
#else
#define FILE_VALID_RES_DIR "LBA2.HQR"
#endif

// --- Private state -----------------------------------------------------------
bool directoriesInitialized = false;
char directoriesUserDir[ADELINE_MAX_PATH] = "";
char directoriesResDir[ADELINE_MAX_PATH] = "";
char directoriesCfgDir[ADELINE_MAX_PATH] = "";
char directoriesCDPrefix[ADELINE_MAX_PATH] = "";
S32 directoriesDistVersion;
/* Set from the command line before anything asks for a path; empty means the
   environment, then the per-user default, decide. See GetDefaultUserDir. */
static char directoriesUserDirOverride[ADELINE_MAX_PATH] = "";
/* Named profile, or empty for the default one, which is the user directory
   itself. See GetDefaultUserDir. */
static char directoriesProfile[ADELINE_MAX_PATH] = "";
/* The marker that put this run in a portable tree, so its one key can be read
   back after the root is settled. Empty when no marker chose the root. */
static char directoriesPortableMarkerPath[ADELINE_MAX_PATH] = "";
/* How the user directory got resolved, for the boot banner. Set once, by
   GetDefaultUserDir. */
static const char *directoriesUserDirSource = "default";

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
/* One sweep of the case variations, asking `exists` about each in turn. Leaves
   the winning spelling in `variation`. */
static bool DiscoverPass(char *variation, U16 variationMaxSize,
                         const char *baseDir, const char *checkPath,
                         bool (*exists)(const char *)) {
    char fullPath[ADELINE_MAX_PATH];

    strncpy(variation, checkPath, variationMaxSize);
    variation[variationMaxSize - 1] = '\0'; // Guarantee

    // Try as is
    snprintf(fullPath, ADELINE_MAX_PATH, "%s%s", baseDir, variation);
    if (exists(fullPath))
        return true;

    // Try uppercase
    SDL_strupr(variation);
    snprintf(fullPath, ADELINE_MAX_PATH, "%s%s", baseDir, variation);
    if (exists(fullPath))
        return true;

    // Try lowercase
    SDL_strlwr(variation);
    snprintf(fullPath, ADELINE_MAX_PATH, "%s%s", baseDir, variation);
    if (exists(fullPath))
        return true;

    // Try title case (first letter uppercase, rest lowercase)
    if (variation[0] >= 'a' && variation[0] <= 'z')
        variation[0] -= 32;
    snprintf(fullPath, ADELINE_MAX_PATH, "%s%s", baseDir, variation);
    return exists(fullPath);
}

bool Discover(char *outFound, U16 foundMaxSize, const char *baseDir,
              const char *checkPath) {
    assert(((foundMaxSize > 0) && (outFound != NULL)) || (foundMaxSize == 0));
    assert(baseDir != NULL);
    assert(checkPath != NULL);

    char variation[ADELINE_MAX_PATH];

    /* Filesystem first, across every spelling, before the disc image gets a
       say. One pass over ExistsFileOrDir would stop at whichever spelling the
       image happens to hold, and a mounted image answers case-insensitively, so
       the very first probe hits and the file actually installed -- under a
       different case -- is never tried. OpenRead then finds nothing on disk at
       that spelling and falls through to the image's copy.

       That is how a GOG install came up in French: it is asked for lba2.cfg,
       has LBA2.CFG on disk saying English, and ships LBA2.GOG, an ISO whose own
       LBA2.CFG leaves Language empty. Installed data has to beat the image,
       which is what OpenRead already promises one level down. */
    bool found = DiscoverPass(variation, ADELINE_MAX_PATH, baseDir, checkPath,
                              ExistsOnFilesystem);
    if (!found) {
        found = DiscoverPass(variation, ADELINE_MAX_PATH, baseDir, checkPath,
                             ExistsFileOrDir);
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

bool IsValidResourceDirOrImage(const char *path) {
    if (IsValidResourceDir(path)) {
        return true;
    }

    // Nothing extracted, but the data may all be inside a disc image sitting in
    // this folder: a retail CD rip is one .bin/.cue and nothing else.
    //
    // Deliberately NOT folded into IsValidResourceDir. This opens every file in
    // the directory to see whether any is an image, which is fine for a path
    // somebody named and ruinous for the speculative sweep: discovery tries up
    // to kMaxTried candidates, including a scan of the siblings of the parent of
    // the working directory, and turning each of those from a stat into a
    // directory walk is enough to hang a CI runner whose build tree happens to
    // sit in the path. Callers on the speculative side must stay on the cheap
    // check above.
    return DiscImage_DirHoldsMarker(path, FILE_VALID_RES_DIR);
}

const char *Directories_GetResMarker(void) { return FILE_VALID_RES_DIR; }

bool Directories_IsUnderResDir(const char *path) {
    // Every Get*Path under the resource tree is built by appending to
    // directoriesResDir, so a prefix match identifies installed game data.
    // A false positive only ever *spares* a file from deletion (the safe
    // direction), so the simple prefix test is deliberate.
    if (path == NULL || directoriesResDir[0] == '\0') {
        return false;
    }
    return strncmp(path, directoriesResDir, strlen(directoriesResDir)) == 0;
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
        Log_Error("Invalid user directory '%s'", directoriesUserDir);
        exit(EXIT_FAILURE);
    }

    strncpy(directoriesResDir, resDir, ADELINE_MAX_PATH);
    directoriesResDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
    // Image-aware: discovery may have accepted a folder whose data is only
    // inside a disc image, and this runs once, on the folder already chosen.
    if (!IsValidResourceDirOrImage(directoriesResDir)) {
        Log_Error("Invalid resource directory '%s'", directoriesResDir);
        exit(EXIT_FAILURE);
    }

    strncpy(directoriesCfgDir, cfgDir, ADELINE_MAX_PATH);
    directoriesCfgDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
    if (!ExistsFileOrDir(directoriesCfgDir)) {
        Log_Error("Invalid config directory '%s'", directoriesCfgDir);
        exit(EXIT_FAILURE);
    }

    strncpy(directoriesCDPrefix, CDPrefix, ADELINE_MAX_PATH);
    directoriesCDPrefix[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
    if (strlen(directoriesCDPrefix) > 0) {
        if (!ExistsFileOrDir(directoriesCDPrefix)) {
            Log_Error("Invalid CD drive or directory '%s'", directoriesCDPrefix);
            exit(EXIT_FAILURE);
        }
    }

    directoriesDistVersion = distVersion;

    directoriesInitialized = true;
}

void EndDirectories() { directoriesInitialized = false; }

// --- Interface ---------------------------------------------------------------
void GetCurrentRunningDir(char *outDir) {
#ifndef LBA2_BUILD_TESTS
    static bool initialized = false;
    static char cachedDir[ADELINE_MAX_PATH] = "";
    if (initialized) {
        strncpy(outDir, cachedDir, ADELINE_MAX_PATH);
        return;
    }
#endif

    char *sdlBasePath = SDL_GetCurrentDirectory();
    if (sdlBasePath == NULL) {
        outDir[0] = '\0';
        return;
    }
    if (SDL_strlen(sdlBasePath) >= ADELINE_MAX_PATH) {
        Log_Error("Current directory path too long (>= %d)", ADELINE_MAX_PATH);
        SDL_free(sdlBasePath);
        outDir[0] = '\0';
        return;
    }
    strncpy(outDir, sdlBasePath, ADELINE_MAX_PATH);
    outDir[ADELINE_MAX_PATH - 1] = '\0';
    SDL_free(sdlBasePath);

#ifndef LBA2_BUILD_TESTS
    strncpy(cachedDir, outDir, ADELINE_MAX_PATH);
    initialized = true;
#endif
}

bool Directories_IsValidProfileName(const char *name) {
    if ((name == NULL) || (name[0] == '\0')) {
        return false;
    }

    /* A profile is a directory inside the user directory and has to stay there,
       so nothing that could walk out of it is accepted: no separator in either
       spelling (a Windows path uses one, a POSIX one the other, and a name
       crossing platforms should be refused on both), and no parent hop. A bare
       dot names the profiles directory itself. */
    if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) {
        return false;
    }
    if (strcmp(name, ".") == 0) {
        return false;
    }

    /* Refused on every platform, for the same reason both separators are: a
       name has to mean the same folder wherever the tree is opened.

       A colon is a drive or a stream on Windows, so `C:` resolves to somewhere
       else entirely and the run dies later with a path nobody typed. A trailing
       dot or space is dropped by the Windows path parser, which quietly makes
       "gog." and "gog" one profile while the player believes they have two.
       Both accepted here on Linux would be a name that stops working when the
       stick is carried across. */
    if (strchr(name, ':')) {
        return false;
    }
    const size_t len = strlen(name);
    if ((name[len - 1] == '.') || (name[len - 1] == ' ')) {
        return false;
    }
    return true;
}

const char *Directories_GetProfile(void) { return directoriesProfile; }

const char *Directories_GetUserDirSource(void) { return directoriesUserDirSource; }

void Directories_SetProfileOverride(const char *name) {
    if (name == NULL) {
        directoriesProfile[0] = '\0';
        return;
    }
    strncpy(directoriesProfile, name, ADELINE_MAX_PATH);
    directoriesProfile[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
}

void Directories_SetUserDirOverride(const char *path) {
    if (path == NULL) {
        directoriesUserDirOverride[0] = '\0';
        return;
    }
    strncpy(directoriesUserDirOverride, path, ADELINE_MAX_PATH);
    directoriesUserDirOverride[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
}

bool Directories_ResolveUserDir(char *outDir, U16 outMax,
                                const char *overridePath, const char *envPath) {
    assert((outDir != NULL) && (outMax > 0));

    const char *chosen = NULL;
    if ((overridePath != NULL) && (overridePath[0] != '\0')) {
        chosen = overridePath;
    } else if ((envPath != NULL) && (envPath[0] != '\0')) {
        chosen = envPath;
    }
    if (chosen == NULL) {
        return false;
    }

    /* Too long to hold, or too long to hold a separator: either way the result
       would be a different directory, and a run that quietly wrote to a
       different directory is what this whole path exists to prevent. Report
       nothing resolved rather than something nearly right. */
    const size_t want = strlen(chosen);
    if (want + 2 > (size_t)outMax) {
        Log_Error("User directory path too long (>= %d): '%s'", outMax, chosen);
        return false;
    }

    strncpy(outDir, chosen, outMax);
    outDir[outMax - 1] = '\0'; // Guarantee

    /* Every Get*Path builds its result as baseDir + name with no separator of
       its own, and SDL_GetPrefPath always hands back a trailing one, so a path
       typed without it has to be brought to the same shape or the first append
       lands a sibling file instead of a child. The length guard above means
       there is always room for it. */
    const size_t len = strlen(outDir);
    if ((len > 0) && (outDir[len - 1] != '/') &&
        (outDir[len - 1] != ADELINE_PATH_SEP_CHAR)) {
        outDir[len] = ADELINE_PATH_SEP_CHAR;
        outDir[len + 1] = '\0';
    }
    return true;
}

/* The marker, however it is spelled. One pass over the directory the binary
   sits in, which is small and read once per boot. */
static bool FindPortableMarker(const char *dir, char *outName, U16 outMax) {
    DIR *dirp = opendir(dir);
    if (dirp == NULL) {
        return false;
    }

    bool found = false;
    struct dirent *entry;
    while (!found && ((entry = readdir(dirp)) != NULL)) {
        if (SDL_strcasecmp(entry->d_name, ADELINE_PORTABLE_MARKER) == 0) {
            strncpy(outName, entry->d_name, outMax);
            outName[outMax - 1] = '\0'; // Guarantee
            found = true;
        }
    }
    closedir(dirp);
    return found;
}

/* The one key the marker carries. A bundle that is somebody's GOG setup can say
   so, which is the only thing about a portable tree that no flag can express: a
   flag belongs to a run, this belongs to the tree and travels with it.

   Parsed here rather than through DefFile, which needs a buffer and sets module
   state this early in the boot, for one key in a file that is usually empty.
   Everything else in it is still reserved and still unread. */
static bool ReadPortableProfile(const char *markerPath, char *outName, U16 outMax) {
    FILE *f = fopen(markerPath, "rb");
    if (f == NULL) {
        return false;
    }

    char line[ADELINE_MAX_PATH];
    bool found = false;
    while (!found && (fgets(line, (int)sizeof(line), f) != NULL)) {
        char *p = line;
        while ((*p == ' ') || (*p == '\t')) {
            p++;
        }
        if (SDL_strncasecmp(p, "profile", 7) != 0) {
            continue;
        }
        p += 7;
        while ((*p == ' ') || (*p == '\t')) {
            p++;
        }
        if ((*p != ':') && (*p != '=')) {
            continue; // a longer word starting "profile", not the key
        }
        p++;
        while ((*p == ' ') || (*p == '\t')) {
            p++;
        }
        /* To end of line, then back over trailing blanks. */
        char *end = p;
        while ((*end != '\0') && (*end != '\r') && (*end != '\n')) {
            end++;
        }
        while ((end > p) && ((end[-1] == ' ') || (end[-1] == '\t'))) {
            end--;
        }
        *end = '\0';
        if (p != end) {
            strncpy(outName, p, outMax);
            outName[outMax - 1] = '\0'; // Guarantee
            found = true;
        }
    }

    fclose(f);
    return found;
}

bool Directories_PortableUserDir(char *outDir, U16 outMax, const char *baseDir) {
    assert((outDir != NULL) && (outMax > 0));

    if ((baseDir == NULL) || (baseDir[0] == '\0')) {
        return false;
    }

    char root[ADELINE_MAX_PATH];
    if (!Directories_ResolveUserDir(root, ADELINE_MAX_PATH, baseDir, NULL)) {
        return false; // too long to hold a separator, so nothing sits beside it
    }

    /* Matched however it is spelled, by looking at what is actually there.
       Discover's four case variations are the right tool for a name a
       distribution shipped, where the spelling is one of a known few. This file
       is the one a player types by hand, on a filesystem that does not care
       about case, and then carries to one that does: portable.TXT and
       pORTABLE.txt are neither of them among those four, and a miss is silent.
       The tree simply is not portable, with nothing said. */
    char marker[ADELINE_MAX_PATH];
    if (!FindPortableMarker(root, marker, ADELINE_MAX_PATH)) {
        return false;
    }
    char markerPath[ADELINE_MAX_PATH];
    if (snprintf(markerPath, ADELINE_MAX_PATH, "%s%s", root, marker) >= ADELINE_MAX_PATH) {
        return false;
    }
    if (IsDirectory(markerPath)) {
        return false;
    }

    /* One directory per build under it, keyed the same way the per-user path is,
       so `User/LBA2/` and `Twinsen/LBA2/` say the same thing and a tree holding
       two builds needs no second scheme. */
    if (snprintf(outDir, outMax, "%s%s%s%s%s", root, ADELINE_PORTABLE_DIR,
                 ADELINE_PATH_SEP, ADELINE_PREF_APP,
                 ADELINE_PATH_SEP) >= (int)outMax) {
        return false;
    }

    strncpy(directoriesPortableMarkerPath, markerPath, ADELINE_MAX_PATH);
    directoriesPortableMarkerPath[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
    return true;
}

bool Directories_PortableProfile(char *outName, U16 outMax) {
    assert((outName != NULL) && (outMax > 0));

    if (directoriesPortableMarkerPath[0] == '\0') {
        return false; // no marker chose this run's root
    }
    return ReadPortableProfile(directoriesPortableMarkerPath, outName, outMax);
}

void GetDefaultUserDir(char *outDir) {
    static bool initialized = false;
    static char userDir[ADELINE_MAX_PATH] = "";
    if (!initialized) {
        /* Explicit override first, environment second, per-user default last:
           the same CLI > env > default order the resolution and game-data paths
           already follow.

           A named directory is created rather than required to exist, matching
           SDL_GetPrefPath, which creates its own. That is what makes
           `--user-dir <fresh path>` an empty profile in one step, with no mkdir
           on the caller. Creation failure is deliberately not papered over with
           a fall back to the default: a run that quietly wrote somewhere other
           than where it was told is the failure this flag exists to prevent, so
           the unusable path stays and InitDirectories stops the boot naming it. */
        const char *env = getenv("LBA2_USER_DIR");
        const bool asked = (directoriesUserDirOverride[0] != '\0') ||
                           ((env != NULL) && (env[0] != '\0'));
        if (directoriesUserDirOverride[0] != '\0') {
            directoriesUserDirSource = "--user-dir";
        } else if (asked) {
            directoriesUserDirSource = "LBA2_USER_DIR";
        }

        if (!Directories_ResolveUserDir(userDir, ADELINE_MAX_PATH,
                                        directoriesUserDirOverride, env)) {
            /* Somebody named a folder and it could not be made into one, which
               is the case this flag exists to make loud. Falling through to the
               per-user default here would write the run's saves somewhere other
               than where it was told while the banner still named the override.
               Empty stops the boot in InitDirectories instead. */
            if (asked) {
                userDir[0] = '\0';
            } else {
                /* A marker beside the binary makes the tree self-contained. It
                   sits below both explicit inputs on purpose, so it changes only
                   the fallback: a file the player may have forgotten about must
                   not override something they typed.

                   SDL3 hands back an internal cached pointer; do not free it. */
                if (Directories_PortableUserDir(userDir, ADELINE_MAX_PATH, SDL_GetBasePath())) {
                    directoriesUserDirSource = ADELINE_PORTABLE_MARKER;
                } else {
                    char *sdlUserPath = SDL_GetPrefPath(ADELINE_PREF_ORG, ADELINE_PREF_APP);
                    if (sdlUserPath != NULL) {
                        strncpy(userDir, sdlUserPath, ADELINE_MAX_PATH);
                        userDir[ADELINE_MAX_PATH - 1] = '\0';
                        SDL_free(sdlUserPath);
                    }
                }
            }
        }

        /* A tree that names its own profile gets it when nothing else did. The
           marker belongs to the tree, so it sits below both the flag and the
           environment, which belong to a run. A name it cannot use stops the
           boot rather than quietly landing in the default profile: writing
           somewhere other than the file asked for is the failure this whole
           path exists to prevent. */
        if ((userDir[0] != '\0') && (directoriesProfile[0] == '\0')) {
            char fromMarker[ADELINE_MAX_PATH];
            if (Directories_PortableProfile(fromMarker, ADELINE_MAX_PATH)) {
                if (Directories_IsValidProfileName(fromMarker)) {
                    Directories_SetProfileOverride(fromMarker);
                } else {
                    Log_Error("%s names '%s', which is not a usable profile name",
                              ADELINE_PORTABLE_MARKER, fromMarker);
                    userDir[0] = '\0';
                }
            }
        }

        /* A named profile is a directory inside whatever root was just
           resolved, so --user-dir and --profile compose: the first says where,
           the second says which. The default profile is the root itself rather
           than a "default" folder under it, so an install that never names one
           keeps writing exactly where it always did and there is nothing to
           migrate. */
        if ((directoriesProfile[0] != '\0') && (userDir[0] != '\0')) {
            char root[ADELINE_MAX_PATH];
            strncpy(root, userDir, ADELINE_MAX_PATH);
            root[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee

            if (snprintf(userDir, ADELINE_MAX_PATH, "%s%s%s%s", root,
                         DIR_NAME_PROFILES, directoriesProfile,
                         ADELINE_PATH_SEP) >= ADELINE_MAX_PATH) {
                /* Truncated, and a truncated path is a different folder. Empty
                   rather than nearly-right, so the boot stops instead of
                   writing somewhere nobody asked for. */
                Log_Error("Profile path too long (>= %d): '%s' under '%s'",
                          ADELINE_MAX_PATH, directoriesProfile, root);
                userDir[0] = '\0';
            }
        }

        /* Idempotent, and it succeeds when the folder is already there, so this
           covers a named user directory, a named profile, and the per-user
           default SDL already created. */
        if (userDir[0] != '\0') {
            SDL_CreateDirectory(userDir);
        }
        initialized = true;
    }

    strncpy(outDir, userDir, ADELINE_MAX_PATH);
}

void GetDefaultResDir(char *outDir) { GetCurrentRunningDir(outDir); }

void GetDefaultCfgPath(char *outPath, U16 pathMaxSize, const char *cfgFilename) {
    assert(directoriesInitialized);
    assert((outPath != NULL) && (pathMaxSize > 0));

    DiscoverEndingAndAppend(outPath, pathMaxSize, directoriesResDir, cfgFilename);
}

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

/* Saves the install ships, handed to a profile the first time it is used.
   `<gameDir>/SAVE/` is where the DOS engine wrote, so every install has the
   folder; retail ones leave it empty, and the 1997 demo fills it with the three
   sections its fixed slots are meant to load. Nothing looked there, so those
   three were unreachable: the slots resolved to files that did not exist.

   Copied rather than read in place, because these are slots the player saves
   back into, and the install may be a mounted image or read-only media.

   Once, keyed on the save folder not existing yet, which is the only moment a
   profile has never been used. Seeding on a missing file instead would raise
   saves the player had deleted, every launch. */
void Directories_SeedSavesFromGameData(void) {
    assert(directoriesInitialized);

    char destDir[ADELINE_MAX_PATH];
    GetSavePath(destDir, ADELINE_MAX_PATH, NULL);
    if (ExistsFileOrDir(destDir)) {
        return; // this profile has been used before
    }

    char srcDir[ADELINE_MAX_PATH];
    DiscoverEndingAndAppend(srcDir, ADELINE_MAX_PATH, directoriesResDir, DIR_NAME_SAVE);
    DIR *dirp = opendir(srcDir);
    if (dirp == NULL) {
        return; // nothing shipped, which is the ordinary case
    }

    S32 copied = 0;
    S32 failed = 0;
    struct dirent *entry;
    while ((entry = readdir(dirp)) != NULL) {
        const char *name = entry->d_name;
        const size_t len = strlen(name);
        /* Top-level saves only. The SHOOT/ beside them holds screenshots the
           engine generates, and a directory is not a save. */
        if (len < 5 || SDL_strcasecmp(name + len - 4, ".lba") != 0) {
            continue;
        }

        char srcPath[ADELINE_MAX_PATH];
        char destPath[ADELINE_MAX_PATH];
        if (snprintf(srcPath, ADELINE_MAX_PATH, "%s%s", srcDir, name) >= ADELINE_MAX_PATH ||
            snprintf(destPath, ADELINE_MAX_PATH, "%s%s", destDir, name) >= ADELINE_MAX_PATH) {
            continue;
        }
        if (!ExistsOnFilesystem(srcPath)) {
            continue; // a directory ending in .lba, or a stale entry
        }
        if (copied == 0 && !SDL_CreateDirectory(destDir)) {
            break; // nowhere to put them; the boot reports the folder elsewhere
        }
        if (Copy(srcPath, destPath)) {
            copied++;
        } else {
            Log_Warn("Saves      could not copy %s out of the game data", name);
            failed++;
        }
    }
    closedir(dirp);

    if (copied > 0) {
        Log_Info("Saves      %d shipped with the game data, copied in", copied);
    } else if (failed > 0) {
        /* The folder was created for a copy that never landed, and its presence
           is what says this profile has been used. Leaving it would retire the
           seeding for good on the first bad run, so take it back and let the
           next launch try again. */
        SDL_RemovePath(destDir); // empty, so this succeeds and the next launch retries
    }
}

/* Make `path` a directory, creating any missing parent on the way, and report whether
   one is there when this returns.

   SDL rather than a walk over the separators, which is what this was. That walk had a
   Windows-shaped hole: the first component of an absolute path there is a drive spec,
   and `_mkdir("C:")` asks for the current directory *of drive C*, which is refused
   outright when the process sits on another drive or on that drive's root. The walk
   stopped at the refusal, skipped the mkdir it existed to perform, and returned a
   failure no caller read, so the folder was simply absent and the first write into it
   failed instead -- naming a file, in a message that could not say the folder above it
   was never made. Neither side of the tree reproduced it: an absolute path on Linux
   begins with the separator, whose empty first component the walk skipped, and a
   Windows run whose build tree and user directory share a drive resolves `C:` to a
   directory that exists.

   SDL_CreateDirectory creates the parents itself, reports success when the directory is
   already there, and knows what a drive spec is. It also creates with 0770 against the
   0754 the walk asked for, so a folder made from now on is one group-readable step
   tighter and matches the user directory above it, which SDL has always made. */
U8 AddDirIfNot(const char *path) {
    /* So that a caller reporting a failure reads this attempt's reason rather than
       whatever last set one, which under SDL is as likely to be a call that succeeded. */
    SDL_ClearError();

    /* Ahead of the create, so a path that exists as a file still answers no. That is
       what a caller testing this return has always been told, and SDL is not asked to
       have an opinion about it. */
    if (ExistsFileOrDir(path)) {
        return IsDirectory(path);
    }

    return SDL_CreateDirectory(path) ? TRUE : FALSE;
}

/* Beside the saves rather than under them, because a recording is a thing the player
   made rather than something about a save: it carries its own savegames, and one
   recording stands in for a whole session. `shoot` and `bugs` sit inside `save` because
   they are debris from a run; this is not. */
void GetRecordingsPath(char *outPath, U16 pathMaxSize, const char *recFilename) {
    assert(directoriesInitialized);
    assert((outPath != NULL) && (pathMaxSize > 0));

    static bool initialized = false;
    static char recordingsPath[ADELINE_MAX_PATH] = "";
    if (!initialized) {
        DiscoverEndingAndAppend(recordingsPath, ADELINE_MAX_PATH, directoriesUserDir,
                                DIR_NAME_RECORDINGS);
        initialized = true;
    }

    DiscoverEndingAndAppend(outPath, pathMaxSize, recordingsPath, recFilename);
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

void GetBugPath(char *outPath, U16 pathMaxSize, const char *bugFilename) {
    assert(directoriesInitialized);
    assert((outPath != NULL) && (pathMaxSize > 0));

    static bool initialized = false;
    static char bugPath[ADELINE_MAX_PATH] = "";
    if (!initialized) {
        char saveDir[ADELINE_MAX_PATH] = "";
        GetSavePath(saveDir, ADELINE_MAX_PATH, "");

        DiscoverEndingAndAppend(bugPath, ADELINE_MAX_PATH, saveDir,
                                DIR_NAME_BUGS);
        initialized = true;
    }

    DiscoverEndingAndAppend(outPath, pathMaxSize, bugPath, bugFilename);
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

void GetLogPathInDir(char *outPath, U16 pathMaxSize, const char *userDir,
                     const char *logFilename) {
    assert((outPath != NULL) && (pathMaxSize > 0));
    assert(userDir != NULL);

    DiscoverEndingAndAppend(outPath, pathMaxSize, userDir, logFilename);
}

void GetLogPath(char *outPath, U16 pathMaxSize, const char *logFilename) {
    assert(directoriesInitialized);
    GetLogPathInDir(outPath, pathMaxSize, directoriesUserDir, logFilename);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
