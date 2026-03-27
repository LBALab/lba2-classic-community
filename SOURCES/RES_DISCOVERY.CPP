#include "RES_DISCOVERY.H"

#include "DIRECTORIES.H"

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/LOGPRINT.H>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include <stdlib.h>
#include <string.h>

namespace {

const int kMaxTried = 48;
const int kParentWalkMax = 8;

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
    snprintf(buf, ADELINE_MAX_PATH, "%s%s%s", baseBuf, sub, ADELINE_PATH_SEP);
    buf[ADELINE_MAX_PATH - 1] = '\0';
    return NormalizeAndTry(buf, outDir, outMax, tried, triedCount);
}

} // namespace

#ifdef __cplusplus
extern "C" {
#endif

bool ResolveGameDataDir(char *outDir, U16 outMax, int *argc, char **argv) {
    char tried[kMaxTried][ADELINE_MAX_PATH];
    int triedCount = 0;
    char cliPath[ADELINE_MAX_PATH];
    char walk[ADELINE_MAX_PATH];
    const char *basePath = NULL;
    bool sdlInitOk = false;

    ExtractGameDirFromArgv(*argc, argv, cliPath, ADELINE_MAX_PATH);
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

    sdlInitOk = SDL_Init(0);
    if (!sdlInitOk) {
        LogPrintf("Warning: SDL_Init(0) failed for game path discovery: %s\n",
                  SDL_GetError());
    }

    basePath = SDL_GetBasePath();
    if (basePath != NULL) {
        if (TryJoinBaseSub(basePath, "", outDir, outMax, tried, &triedCount) ||
            TryJoinBaseSub(basePath, "data", outDir, outMax, tried, &triedCount) ||
            TryJoinBaseSub(basePath, "game", outDir, outMax, tried, &triedCount)) {
            SDL_free((void *)basePath);
            basePath = NULL;
            if (sdlInitOk) {
                SDL_Quit();
            }
            return true;
        }
        SDL_free((void *)basePath);
        basePath = NULL;
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    if (NormalizeAndTry(walk, outDir, outMax, tried, &triedCount)) {
        if (sdlInitOk) {
            SDL_Quit();
        }
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
            if (sdlInitOk) {
                SDL_Quit();
            }
            return true;
        }
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    {
        char rel[ADELINE_MAX_PATH];
        snprintf(rel, ADELINE_MAX_PATH, "%s%s%s", walk, "data", ADELINE_PATH_SEP);
        rel[ADELINE_MAX_PATH - 1] = '\0';
        if (NormalizeAndTry(rel, outDir, outMax, tried, &triedCount)) {
            if (sdlInitOk) {
                SDL_Quit();
            }
            return true;
        }
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    {
        char rel[ADELINE_MAX_PATH];
        snprintf(rel, ADELINE_MAX_PATH, "%s..%s%s%s", walk, ADELINE_PATH_SEP, "LBA2",
                 ADELINE_PATH_SEP);
        rel[ADELINE_MAX_PATH - 1] = '\0';
        if (NormalizeAndTry(rel, outDir, outMax, tried, &triedCount)) {
            if (sdlInitOk) {
                SDL_Quit();
            }
            return true;
        }
    }

    GetCurrentRunningDir(walk);
    walk[ADELINE_MAX_PATH - 1] = '\0';
    EnsureTrailingSeparator(walk, ADELINE_MAX_PATH);
    {
        char rel[ADELINE_MAX_PATH];
        snprintf(rel, ADELINE_MAX_PATH, "%s..%s%s%s", walk, ADELINE_PATH_SEP, "game",
                 ADELINE_PATH_SEP);
        rel[ADELINE_MAX_PATH - 1] = '\0';
        if (NormalizeAndTry(rel, outDir, outMax, tried, &triedCount)) {
            if (sdlInitOk) {
                SDL_Quit();
            }
            return true;
        }
    }

    if (sdlInitOk) {
        SDL_Quit();
    }

    LogFailure(tried, triedCount);
    return false;
}

#ifdef __cplusplus
}
#endif
