#include "DIRECTORIES.H"

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/ANDROID.H>
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
#include <time.h>

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
/* The bare names exist because two of them are also what says "this folder is
   somebody's user directory" in Directories_HasUserData, and a probe there has
   to be a path with no trailing separator: Windows' _stat refuses one. */
#define DIR_NAME_PROFILES_BASE "profiles"
#define DIR_NAME_SAVE_BASE "save"
#define DIR_NAME_PROFILES DIR_NAME_PROFILES_BASE ADELINE_PATH_SEP
#define DIR_NAME_SAVE DIR_NAME_SAVE_BASE ADELINE_PATH_SEP
/* The third mark of a used user directory, and the only one this module does
   not already name. It is CFG_NAME in COMMON.H, spelled again here rather than
   included: COMMON.H is not standalone -- it names engine types and pulls the
   god header behind it, which is a lot of coupling to buy one string. The
   config file has carried this name since 1997, and a rename that missed this
   line would cost a settings-only player their key bindings, not their saves. */
#define FILE_NAME_CFG "lba2.cfg"
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
/* Two things worth saying about resolving the root that cannot be said while it
   happens: the log file lives inside the folder being chosen, so nothing is
   listening yet. Held here for the banner to read back. */
static char directoriesUnwritableRoot[ADELINE_MAX_PATH] = "";
static char directoriesMigratedFrom[ADELINE_MAX_PATH] = "";
static char directoriesRivalUserDir[ADELINE_MAX_PATH] = "";
static bool directoriesMigrationPartial = false;

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

const char *Directories_GetUnwritableRoot(void) { return directoriesUnwritableRoot; }

const char *Directories_GetMigratedFrom(void) { return directoriesMigratedFrom; }

bool Directories_MigrationWasPartial(void) { return directoriesMigrationPartial; }

const char *Directories_GetRivalUserDir(void) { return directoriesRivalUserDir; }

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

/* --- Where a run may write --------------------------------------------------

   Beside the game data rather than inside it: the retail HQR set is read-only
   data a player may re-push at any time, and their saves must not be sitting in
   the folder they overwrite. One level down from `/sdcard/lba2cc/` keeps both
   in one thing to back up while keeping "yours" and "the game's" apart on
   sight.

   Shared storage rather than the app-specific folder, which is what the
   platform's own guidance would pick. Two facts decide it the other way. The
   app-specific external folder is erased by an uninstall exactly as internal
   storage is, so it does not survive the one event that costs a player their
   saves; and since Android 11 a file manager cannot browse Android/data, so a
   player cannot reach their own saves to copy them off or send them with a
   report.

   The sanctioned alternative is the Storage Access Framework, and it is not a
   drop-in for an engine built on paths. SAF routes each operation through IPC
   and is orders of magnitude slower than the file API, which this engine's many
   small reads would feel; and Android TV has no folder picker to grant a
   directory with, which would leave the leanback build nowhere to write. The
   price of choosing against the guidance is that MANAGE_EXTERNAL_STORAGE is a
   restricted permission on Google Play and a game is not an eligible category,
   so this layout and Play distribution cannot both be had.

   One directory per build under `user/`, keyed by ADELINE_PREF_APP exactly as
   the per-user and portable roots are, so `/sdcard/lba2cc/user/LBA2/`,
   `User/LBA2/` and `Twinsen/LBA2/` all say the same thing. Without it a demo
   build and a retail one would share a save folder here while being kept apart
   everywhere else, and a second game would have nowhere of its own to go. */
#define ANDROID_USER_DIR_SDCARD "/sdcard/lba2cc/user/" ADELINE_PREF_APP
/* The same folder under the name it carries when /sdcard is not the symlink it
   is on nearly every device. ResolveGameDataDir probes both spellings for the
   game data; writing follows it rather than inventing a second rule. */
#define ANDROID_USER_DIR_EMULATED "/storage/emulated/0/lba2cc/user/" ADELINE_PREF_APP

/* A user directory is a handful of files and two levels of folders. The cap
   stops a tree that somehow links back into itself rather than limiting
   anything a player can accumulate. */
#define USER_DIR_COPY_MAX_DEPTH 8

/* Settled by writing, for the reason the header gives. The probe is removed
   again either way, so a candidate that wins leaves nothing behind and one that
   loses leaves nothing either. */
static bool CanWriteInDir(const char *dir) {
    if (!AddDirIfNot(dir)) {
        return false;
    }

    char probe[ADELINE_MAX_PATH];
    if (snprintf(probe, ADELINE_MAX_PATH, "%s%s", dir, ".lba2cc-write-probe") >=
        ADELINE_MAX_PATH) {
        return false;
    }

    FILE *f = fopen(probe, "wb");
    if (f == NULL) {
        return false;
    }
    const bool wrote = (fputc('.', f) != EOF);
    fclose(f);
    SDL_RemovePath(probe);
    return wrote;
}

S32 Directories_SelectWritableRoot(const char *const *candidates, S32 count,
                                   char *outDir, U16 outMax) {
    assert((outDir != NULL) && (outMax > 0));
    if (candidates == NULL) {
        return -1;
    }

    for (S32 i = 0; i < count; i++) {
        char root[ADELINE_MAX_PATH];
        /* Borrowed for the one job it does here: bring the candidate to a
           trailing separator, and refuse it outright when there is no room for
           one. A NULL or empty candidate falls out the same way. */
        if (!Directories_ResolveUserDir(root, ADELINE_MAX_PATH, candidates[i], NULL)) {
            continue;
        }
        if (strlen(root) + 1 > (size_t)outMax) {
            continue;
        }
        if (!CanWriteInDir(root)) {
            continue;
        }

        strncpy(outDir, root, outMax);
        outDir[outMax - 1] = '\0'; // Guarantee
        return i;
    }
    return -1;
}

bool Directories_HasUserData(const char *dir) {
    char root[ADELINE_MAX_PATH];
    if (!Directories_ResolveUserDir(root, ADELINE_MAX_PATH, dir, NULL)) {
        return false;
    }

    /* Three things only this engine creates. See the header for why this is not
       "the directory is not empty". */
    static const char *const marks[] = {FILE_NAME_CFG, DIR_NAME_SAVE_BASE,
                                        DIR_NAME_PROFILES_BASE};
    for (size_t i = 0; i < sizeof(marks) / sizeof(marks[0]); i++) {
        char path[ADELINE_MAX_PATH];
        if (snprintf(path, ADELINE_MAX_PATH, "%s%s", root, marks[i]) >= ADELINE_MAX_PATH) {
            continue;
        }
        if (ExistsFileOrDir(path)) {
            return true;
        }
    }
    return false;
}

/* Two spellings of one folder, settled by putting a file in one and looking for
   it in the other.

   This is not pedantry: /sdcard is a symlink to /storage/emulated/0 on nearly
   every Android device, so two of the roots ranked here are routinely the same
   directory under different names, and comparing the strings says they differ.
   Without this, a player with saves would be told on every launch that a second
   copy exists somewhere else, and the second copy would be the one they are
   already using.

   realpath answers this on POSIX and not on Windows, and a device/inode compare
   is POSIX-only too. A probe file answers it everywhere, with the same tool this
   module already uses to decide writability. */
static bool SameDirectory(const char *a, const char *b) {
    if (strcmp(a, b) == 0) {
        return true;
    }

    char probeA[ADELINE_MAX_PATH];
    char probeB[ADELINE_MAX_PATH];
    if (snprintf(probeA, ADELINE_MAX_PATH, "%s%s", a, ".lba2cc-alias-probe") >=
            ADELINE_MAX_PATH ||
        snprintf(probeB, ADELINE_MAX_PATH, "%s%s", b, ".lba2cc-alias-probe") >=
            ADELINE_MAX_PATH) {
        return false;
    }

    /* Removed before the answer is returned either way, so nothing is left for a
       later run to trip over and read as an alias that is not one.

       The probe carries a token and the answer is that token coming back, not
       merely a file being there. A run killed between the write and the removal
       leaves one behind, and on Android the storage-permission flow puts the
       process into the background moments before this runs, so that is reachable
       rather than theoretical. A leftover would otherwise make two different
       folders read as one and silently skip the migration. */
    static U32 probeSeq = 0;
    char token[64];
    const int tokenLen = snprintf(token, sizeof(token), "%lu-%u", (unsigned long)time(NULL),
                                  ++probeSeq);
    if (tokenLen <= 0 || (size_t)tokenLen >= sizeof(token)) {
        return false;
    }

    FILE *f = fopen(probeA, "wb");
    if (f == NULL) {
        return false; // cannot ask; treat them as different and copy nothing
    }
    const bool wrote = fwrite(token, 1, (size_t)tokenLen, f) == (size_t)tokenLen;
    if (fclose(f) != 0 || !wrote) {
        SDL_RemovePath(probeA);
        return false;
    }

    bool same = false;
    FILE *g = fopen(probeB, "rb");
    if (g != NULL) {
        char got[64];
        const size_t n = fread(got, 1, sizeof(got) - 1, g);
        got[n] = '\0';
        fclose(g);
        same = strcmp(got, token) == 0;
    }
    SDL_RemovePath(probeA);
    return same;
}

/* A folder that has already been copied up carries this, so a later boot can
   tell its own leftover from a second set of saves somebody is still using. */
#define USER_DIR_MIGRATED_MARKER ".lba2cc-migrated"

/* Copy() reads four bytes at a time and counts its loop in bytes rather than in
   reads, so a megabyte costs a million iterations, and it reports failure for an
   empty file. Neither matters for the save slot it was written for. The
   migration moves a whole user directory, recordings and screenshots included,
   before the log or any window exists, so it needs a copy that is neither. */
static bool CopyFileContents(const char *src, const char *dest) {
    FILE *in = fopen(src, "rb");
    if (in == NULL) {
        return false;
    }
    FILE *out = fopen(dest, "wb");
    if (out == NULL) {
        fclose(in);
        return false;
    }

    bool ok = true;
    char buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    if (ferror(in) != 0) {
        ok = false;
    }
    if (fclose(out) != 0) {
        ok = false; // a full disc can surface only here
    }
    fclose(in);
    return ok;
}

static bool MigrationMarkerPath(const char *dir, char *out, U16 outMax) {
    return snprintf(out, outMax, "%s%s", dir, USER_DIR_MIGRATED_MARKER) < (int)outMax;
}

static bool WasAlreadyMigratedFrom(const char *dir) {
    char path[ADELINE_MAX_PATH];
    if (!MigrationMarkerPath(dir, path, ADELINE_MAX_PATH)) {
        return false;
    }
    return ExistsOnFilesystem(path);
}

static void MarkMigratedFrom(const char *dir) {
    char path[ADELINE_MAX_PATH];
    if (!MigrationMarkerPath(dir, path, ADELINE_MAX_PATH)) {
        return;
    }
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fclose(f);
    }
}

/* Both paths arrive with a trailing separator and leave with one, so the
   recursion has a single shape and no step has to ask whether to add one. */
static bool CopyTree(const char *srcDir, const char *destDir, S32 depth) {
    if (depth > USER_DIR_COPY_MAX_DEPTH) {
        return false;
    }

    DIR *dirp = opendir(srcDir);
    if (dirp == NULL) {
        return false;
    }
    if (!AddDirIfNot(destDir)) {
        closedir(dirp);
        return false;
    }

    bool ok = true;
    struct dirent *entry;
    while ((entry = readdir(dirp)) != NULL) {
        const char *name = entry->d_name;
        if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0)) {
            continue;
        }

        char srcPath[ADELINE_MAX_PATH];
        char destPath[ADELINE_MAX_PATH];
        if (snprintf(srcPath, ADELINE_MAX_PATH, "%s%s", srcDir, name) >= ADELINE_MAX_PATH ||
            snprintf(destPath, ADELINE_MAX_PATH, "%s%s", destDir, name) >= ADELINE_MAX_PATH) {
            ok = false;
            continue;
        }

        if (IsDirectory(srcPath)) {
            char srcSub[ADELINE_MAX_PATH];
            char destSub[ADELINE_MAX_PATH];
            if (snprintf(srcSub, ADELINE_MAX_PATH, "%s%s", srcPath, ADELINE_PATH_SEP) >=
                    ADELINE_MAX_PATH ||
                snprintf(destSub, ADELINE_MAX_PATH, "%s%s", destPath, ADELINE_PATH_SEP) >=
                    ADELINE_MAX_PATH) {
                ok = false;
                continue;
            }
            if (!CopyTree(srcSub, destSub, depth + 1)) {
                ok = false;
            }
            continue;
        }

        if (!CopyFileContents(srcPath, destPath)) {
            Log_Warn("User dir   could not copy '%s'", name);
            ok = false;
        }
    }

    closedir(dirp);
    return ok;
}

bool Directories_ReconcileUserDir(const char *chosen, const char *const *lower,
                                  S32 count) {
    assert(chosen != NULL);

    /* This call's findings, not the accumulated history of the process. The
       boot makes one call, so this changes nothing there. It matters to anyone
       calling twice: without it, MigrationWasPartial after a call that copied
       nothing answers the question an earlier call asked. */
    directoriesRivalUserDir[0] = '\0';
    directoriesMigratedFrom[0] = '\0';
    directoriesMigrationPartial = false;

    char dest[ADELINE_MAX_PATH];
    if (!Directories_ResolveUserDir(dest, ADELINE_MAX_PATH, chosen, NULL)) {
        return false;
    }
    /* Somebody has played in the chosen folder. Nothing further down is worth
       overwriting that, and it is what makes every launch after the first a
       no-op -- but the loop still runs, to notice a second folder that also
       holds saves. */
    const bool destUsed = Directories_HasUserData(dest);
    if (lower == NULL) {
        return false;
    }

    for (S32 i = 0; i < count; i++) {
        char src[ADELINE_MAX_PATH];
        if (!Directories_ResolveUserDir(src, ADELINE_MAX_PATH, lower[i], NULL)) {
            continue;
        }
        if (SameDirectory(src, dest)) {
            continue; // one folder under two spellings
        }
        /* The chosen folder lives inside this one. Copying would walk the
           destination into itself, and naming it as a second set of saves would
           point the player at the parent of the folder they are using. Asked
           here, with the other question about what these paths are, rather than
           below the one about what they hold. */
        if (strncmp(dest, src, strlen(src)) == 0) {
            continue;
        }
        if (!Directories_HasUserData(src)) {
            continue;
        }
        if (WasAlreadyMigratedFrom(src)) {
            /* Copied up by an earlier boot and deliberately left in place. It
               holds the same saves the chosen folder does, so calling it a
               second set every launch would be a warning nobody can act on. */
            continue;
        }
        if (destUsed) {
            /* Two real save sets, and no way from here to tell which one the
               player wants. Say so and touch neither. */
            strncpy(directoriesRivalUserDir, src, ADELINE_MAX_PATH);
            directoriesRivalUserDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
            return false;
        }
        /* What lands is still the player's and the original is untouched, so a
           partial copy is worth keeping rather than unwinding. Recorded after
           the fact so the banner can say which of the two happened. */
        const bool whole = CopyTree(src, dest, 0);
        strncpy(directoriesMigratedFrom, src, ADELINE_MAX_PATH);
        directoriesMigratedFrom[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
        directoriesMigrationPartial = !whole;
        if (whole) {
            /* Marked only on a complete copy. Left unmarked otherwise, so the
               folder keeps being reported as holding saves of its own, which
               after a partial copy is exactly what it does. */
            MarkMigratedFrom(src);
        } else {
            Log_Warn("User dir   some files could not be copied out of %s", src);
        }
        return true;
    }
    return false;
}

/* The roots this run may write to, most preferred first.

   On a desktop there is one, and it is the folder SDL picks. On Android that
   folder is /data/data/<pkg>/files/, which no file manager, no attached PC and
   no backup tool can read, and which the system erases on uninstall -- and
   uninstalling is how an Android player gets a new build. So the order there
   runs outward: the folder beside their game data first, the app-specific
   external folder next, and SDL's only if neither can be written to.

   The Android paths are spelled without a platform conditional, the way
   ResolveGameDataDir already spells them. Off Android they name nothing that
   exists and nothing that can be created, so the loop falls through them -- and
   the gate below makes that a statement rather than a lucky accident, because
   Android_GetExternalFilesDir is the one call that answers only there. */
static void ResolveDefaultUserDirRoot(char *outDir, U16 outMax) {
    const char *candidates[4];
    S32 count = 0;

    char appExternal[ADELINE_MAX_PATH] = "";
    char sdlDir[ADELINE_MAX_PATH] = "";

    if (Android_GetExternalFilesDir(appExternal, ADELINE_MAX_PATH)) {
        candidates[count++] = ANDROID_USER_DIR_SDCARD;
        candidates[count++] = ANDROID_USER_DIR_EMULATED;
        candidates[count++] = appExternal;
    }

    /* SDL3 hands back an internal cached pointer; do not free it. */
    char *sdlUserPath = SDL_GetPrefPath(ADELINE_PREF_ORG, ADELINE_PREF_APP);
    if (sdlUserPath != NULL) {
        strncpy(sdlDir, sdlUserPath, ADELINE_MAX_PATH);
        sdlDir[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
        SDL_free(sdlUserPath);
        candidates[count++] = sdlDir;
    }

    const S32 chosen = Directories_SelectWritableRoot(candidates, count, outDir, outMax);
    if (chosen < 0) {
        outDir[0] = '\0';
        return;
    }

    if (chosen > 0) {
        /* Worth saying: on Android this is All Files Access being withheld, and
           it is the difference between saves the player can copy off the device
           and saves they cannot. Silence here is what would make the folder
           look like it moved on its own. */
        strncpy(directoriesUnwritableRoot, candidates[0], ADELINE_MAX_PATH);
        directoriesUnwritableRoot[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee
    }

    /* Everything less preferred than the winner is somewhere this install may
       have written on an earlier launch, or an earlier build. */
    Directories_ReconcileUserDir(outDir, &candidates[chosen + 1], count - chosen - 1);
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
                    ResolveDefaultUserDirRoot(userDir, ADELINE_MAX_PATH);
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
