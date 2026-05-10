#include "RES_DISCOVERY.H"

#include "DIRECTORIES.H"

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/LOGPRINT.H>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

const int kMaxTried = 160;
const int kParentWalkMax = 8;
const int kMaxSiblingEntries = 24;

static const char *const kSiblingSubdirs[] = {
    "CommonClassic",
    "common",
    "Common",
    "Classic",
};

void EnsureTrailingSeparator(char *path, U16 maxLen) {
    const size_t n = strlen(path);
    if (n == 0 || n + 2 > (size_t)maxLen) {
        return;
    }
    const char c = path[n - 1];
    if (c != '/' && c != '\\') {
        strncat(path, ADELINE_PATH_SEP, (size_t)maxLen - n - 1);
        path[maxLen - 1] = '\0';
    }
}

bool PathRemoveLastComponent(char *path) {
    size_t len = strlen(path);
    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        path[--len] = '\0';
    }
    if (len == 0) {
        return false;
    }
    for (size_t i = len; i > 0; i--) {
        if (path[i - 1] == '/' || path[i - 1] == '\\') {
            path[i - 1] = '\0';
            return true;
        }
    }
    return false;
}

bool AlreadyTried(const char *norm, char tried[][ADELINE_MAX_PATH], int count) {
    for (int i = 0; i < count; i++) {
        if (!strcmp(tried[i], norm)) {
            return true;
        }
    }
    return false;
}

bool NormalizeAndTry(const char *pathIn, char *outDir, U16 outMax,
                     char tried[][ADELINE_MAX_PATH], int *triedCount) {
    char buf[ADELINE_MAX_PATH];

    if (pathIn == NULL || pathIn[0] == '\0') {
        return false;
    }

    strncpy(buf, pathIn, ADELINE_MAX_PATH);
    buf[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(buf, ADELINE_MAX_PATH);

    if (AlreadyTried(buf, tried, *triedCount)) {
        return false;
    }

    if (*triedCount >= kMaxTried) {
        return false;
    }

    strncpy(tried[*triedCount], buf, ADELINE_MAX_PATH);
    tried[*triedCount][ADELINE_MAX_PATH - 1] = '\0';
    (*triedCount)++;

    if (!IsValidResourceDir(buf)) {
        return false;
    }

    strncpy(outDir, buf, outMax);
    outDir[outMax - 1] = '\0';
    return true;
}

void LogFailure(const char tried[][ADELINE_MAX_PATH], int triedCount) {
    LogPrintf("Error: No valid game data directory (need lba2.hqr). Tried %d path(s).\n",
              triedCount);
    for (int i = 0; i < triedCount; i++) {
        LogPrintf("  - %s\n", tried[i]);
    }
    LogPuts("Hints: set LBA2_GAME_DIR to your install folder, or use --game-dir <path>.");
    LogPuts("See docs/GAME_DATA.md. Retail game files are not included in this repository.");
}

void StripGameDirArgs(int *argc, char **argv) {
    int writeIdx = 1;
    for (int readIdx = 1; readIdx < *argc; readIdx++) {
        if ((!strcasecmp(argv[readIdx], "--game-dir") ||
             !strcasecmp(argv[readIdx], "--data-dir")) &&
            (readIdx + 1) < *argc) {
            readIdx++;
            continue;
        }
        if (!strcasecmp(argv[readIdx], "--pick-game-dir")) {
            continue;
        }
        argv[writeIdx++] = argv[readIdx];
    }
    argv[writeIdx] = NULL;
    *argc = writeIdx;
}

bool ExtractGameDirFromArgv(int argc, char **argv, char *outPath, U16 outMax) {
    outPath[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if ((!strcasecmp(argv[i], "--game-dir") || !strcasecmp(argv[i], "--data-dir")) &&
            (i + 1) < argc) {
            strncpy(outPath, argv[i + 1], outMax);
            outPath[outMax - 1] = '\0';
            return true;
        }
    }
    return false;
}

/* --pick-game-dir tells ResolveGameDataDir to skip the persisted-LastGameDir
 * probe and the auto-discovery probe chain — go straight to the picker
 * fallback regardless of what's been saved or what's adjacent. Useful for:
 *
 *   - Testing the first-launch UX repeatedly without rm-ing last_game_dir.txt
 *   - Switching to a different LBA2 install without editing the persisted file
 *   - Anyone whose current persisted/auto-discovered path resolves to the
 *     wrong thing and wants a UI re-do
 *
 * Lower priority than --game-dir / LBA2_GAME_DIR (explicit user input wins),
 * higher priority than persisted / auto-discovery / picker default behavior. */
bool ExtractPickGameDirFlag(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcasecmp(argv[i], "--pick-game-dir")) {
            return true;
        }
    }
    return false;
}

bool TryJoinBaseSub(const char *base, const char *sub, char *outDir, U16 outMax,
                    char tried[][ADELINE_MAX_PATH], int *triedCount) {
    char buf[ADELINE_MAX_PATH];
    char baseBuf[ADELINE_MAX_PATH];

    if (base == NULL || base[0] == '\0') {
        return false;
    }
    strncpy(baseBuf, base, ADELINE_MAX_PATH);
    baseBuf[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(baseBuf, ADELINE_MAX_PATH);

    if (sub == NULL || sub[0] == '\0') {
        return NormalizeAndTry(baseBuf, outDir, outMax, tried, triedCount);
    }
    int n = snprintf(buf, ADELINE_MAX_PATH, "%s%s%s", baseBuf, sub,
                     ADELINE_PATH_SEP);
    if (n < 0 || (size_t)n >= ADELINE_MAX_PATH) {
        return false;
    }
    return NormalizeAndTry(buf, outDir, outMax, tried, triedCount);
}

/**
 * Under the parent of the current working directory, try each immediate
 * subdirectory as a game root, then common distributor subfolders (e.g.
 * CommonClassic). Bounded by kMaxSiblingEntries; only directories are
 * considered. Success still requires lba2.hqr (IsValidResourceDir).
 */
bool TryParentSiblingScan(char *outDir, U16 outMax, char tried[][ADELINE_MAX_PATH],
                          int *triedCount) {
    char cwd[ADELINE_MAX_PATH];
    GetCurrentRunningDir(cwd);
    cwd[ADELINE_MAX_PATH - 1] = '\0';
    if (cwd[0] == '\0') {
        return false;
    }
    EnsureTrailingSeparator(cwd, ADELINE_MAX_PATH);
    char parent[ADELINE_MAX_PATH];
    strncpy(parent, cwd, ADELINE_MAX_PATH);
    parent[ADELINE_MAX_PATH - 1] = '\0';
    if (!PathRemoveLastComponent(parent)) {
        return false;
    }
    EnsureTrailingSeparator(parent, ADELINE_MAX_PATH);

    DIR *dirp = opendir(parent);
    if (dirp == NULL) {
        return false;
    }

    int n = 0;
    int iter = 0;
    struct dirent *ent;
    while ((ent = readdir(dirp)) != NULL && iter < 256) {
        iter++;
        if (n >= kMaxSiblingEntries) {
            break;
        }
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' ||
             (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
            continue;
        }
        char childPath[ADELINE_MAX_PATH];
        int cn = snprintf(childPath, ADELINE_MAX_PATH, "%s%s", parent,
                          ent->d_name);
        if (cn < 0 || (size_t)cn >= ADELINE_MAX_PATH) {
            continue;
        }
        if (!IsDirectory(childPath)) {
            continue;
        }
        n++;
        EnsureTrailingSeparator(childPath, ADELINE_MAX_PATH);
        if (TryJoinBaseSub(childPath, "", outDir, outMax, tried, triedCount)) {
            closedir(dirp);
            return true;
        }
        for (size_t si = 0; si < sizeof(kSiblingSubdirs) / sizeof(kSiblingSubdirs[0]);
             si++) {
            if (TryJoinBaseSub(childPath, kSiblingSubdirs[si], outDir, outMax, tried,
                               triedCount)) {
                closedir(dirp);
                return true;
            }
        }
    }
    closedir(dirp);
    return false;
}

/* Persisted-LastGameDir helpers.
 *
 * Single-line text file at <prefPath>/last_game_dir.txt holding the
 * absolute path the user picked via the first-launch folder dialog.
 * Stored separately from lba2.cfg because it must be written immediately
 * at picker-success time, before InitDirectories / InitAdeline have set
 * up the lba2.cfg buffer machinery. Written via stdio; read via stdio.
 *
 * Org/app naming ("Twinsen", "LBA2") matches the existing convention in
 * SOURCES/DIRECTORIES.CPP for SDL_GetPrefPath. */
bool BuildPersistedGameDirPath(char *outPath, size_t outMax) {
    char *prefPath = SDL_GetPrefPath("Twinsen", "LBA2");
    if (prefPath == NULL) {
        return false;
    }
    /* SDL_GetPrefPath always returns a path with a trailing separator. */
    int written = snprintf(outPath, outMax, "%slast_game_dir.txt", prefPath);
    SDL_free(prefPath);
    return written > 0 && static_cast<size_t>(written) < outMax;
}

bool ReadPersistedGameDir(char *outPath, U16 outMax) {
    char filePath[ADELINE_MAX_PATH];
    if (!BuildPersistedGameDirPath(filePath, sizeof(filePath))) {
        return false;
    }

    FILE *fp = fopen(filePath, "r");
    if (fp == NULL) {
        /* No persisted file — first launch or never picked. Silent. */
        return false;
    }

    bool ok = (fgets(outPath, outMax, fp) != NULL);
    fclose(fp);
    if (!ok) {
        return false;
    }

    /* Strip trailing CR/LF. */
    size_t len = strlen(outPath);
    while (len > 0 &&
           (outPath[len - 1] == '\n' || outPath[len - 1] == '\r')) {
        outPath[--len] = '\0';
    }
    return outPath[0] != '\0';
}

} // namespace

#ifdef __cplusplus
extern "C" {
#endif

bool WritePersistedGameDir(const char *path) {
    char filePath[ADELINE_MAX_PATH];
    if (!BuildPersistedGameDirPath(filePath, sizeof(filePath))) {
        return false;
    }

    FILE *fp = fopen(filePath, "w");
    if (fp == NULL) {
        LogPrintf("RES_DISCOVERY: could not write %s\n", filePath);
        return false;
    }

    fprintf(fp, "%s\n", path);
    fclose(fp);
    return true;
}

bool ResolveGameDataDir(char *outDir, U16 outMax, int *argc, char **argv) {
    char tried[kMaxTried][ADELINE_MAX_PATH];
    int triedCount = 0;
    char cliPath[ADELINE_MAX_PATH];
    char persisted[ADELINE_MAX_PATH];
    char walk[ADELINE_MAX_PATH];
    const char *basePath = NULL;

    ExtractGameDirFromArgv(*argc, argv, cliPath, ADELINE_MAX_PATH);
    const bool forcePicker = ExtractPickGameDirFlag(*argc, argv);
    StripGameDirArgs(argc, argv);

    if (cliPath[0] != '\0' &&
        NormalizeAndTry(cliPath, outDir, outMax, tried, &triedCount)) {
        return true;
    }

    const char *env = getenv("LBA2_GAME_DIR");
    if (env != NULL && env[0] != '\0' &&
        NormalizeAndTry(env, outDir, outMax, tried, &triedCount)) {
        return true;
    }

    /* --pick-game-dir bails out of the implicit-discovery chain and
     * forces the caller's picker fallback. Explicit --game-dir / env
     * still win above (user passing both is asking for "use this path,
     * but if it's bad show the picker"). */
    if (forcePicker) {
        LogPuts("RES_DISCOVERY: --pick-game-dir set; skipping persisted "
                "and auto-discovery probes.");
        return false;
    }

    /* Persisted choice from a previous picker session. Higher priority
     * than auto-discovery (the user explicitly picked it) but lower than
     * explicit CLI/env overrides (someone passing --game-dir today wants
     * today's path, not last week's). Silent fallthrough on stale path
     * — picker re-prompts, lba2cc rewrites the file. */
    if (ReadPersistedGameDir(persisted, ADELINE_MAX_PATH) &&
        NormalizeAndTry(persisted, outDir, outMax, tried, &triedCount)) {
        return true;
    }

    /* SDL_Init must be done by the caller before SDL_GetBasePath / cwd helpers
     * (see main in PERSO.CPP). Do not SDL_Quit here — InitAdeline needs SDL.
     *
     * SDL3: SDL_GetBasePath() returns an internal cached pointer; do not SDL_free
     * it (SDL_QuitFilesystem frees it). */

    basePath = SDL_GetBasePath();
    if (basePath != NULL) {
        if (TryJoinBaseSub(basePath, "", outDir, outMax, tried, &triedCount) ||
            TryJoinBaseSub(basePath, "data", outDir, outMax, tried, &triedCount) ||
            TryJoinBaseSub(basePath, "game", outDir, outMax, tried, &triedCount)) {
            return true;
        }
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    if (NormalizeAndTry(walk, outDir, outMax, tried, &triedCount)) {
        return true;
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    for (int depth = 0; depth < kParentWalkMax; depth++) {
        if (!PathRemoveLastComponent(walk)) {
            break;
        }
        EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
        if (NormalizeAndTry(walk, outDir, outMax, tried, &triedCount)) {
            return true;
        }
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    {
        char rel[ADELINE_MAX_PATH];
        int n = snprintf(rel, ADELINE_MAX_PATH, "%s%s%s", walk, "data",
                         ADELINE_PATH_SEP);
        if (n > 0 && (size_t)n < ADELINE_MAX_PATH) {
            if (NormalizeAndTry(rel, outDir, outMax, tried, &triedCount)) {
                return true;
            }
        }
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    {
        char rel[ADELINE_MAX_PATH];
        int n = snprintf(rel, ADELINE_MAX_PATH, "%s..%s%s%s", walk,
                         ADELINE_PATH_SEP, "LBA2", ADELINE_PATH_SEP);
        if (n > 0 && (size_t)n < ADELINE_MAX_PATH) {
            if (NormalizeAndTry(rel, outDir, outMax, tried, &triedCount)) {
                return true;
            }
        }
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    {
        char rel[ADELINE_MAX_PATH];
        int n = snprintf(rel, ADELINE_MAX_PATH, "%s..%s%s%s", walk,
                         ADELINE_PATH_SEP, "game", ADELINE_PATH_SEP);
        if (n > 0 && (size_t)n < ADELINE_MAX_PATH) {
            if (NormalizeAndTry(rel, outDir, outMax, tried, &triedCount)) {
                return true;
            }
        }
    }

    if (TryParentSiblingScan(outDir, outMax, tried, &triedCount)) {
        return true;
    }

    LogFailure(tried, triedCount);
    return false;
}

#ifdef __cplusplus
}
#endif
