/**
 * Host-only tests for game data path resolution (no Docker / no ASM).
 *
 * Asset root: the resolved directory is the single `directoriesResDir`; all
 * HQR/music/video paths are relative to it (see GetResPath in DIRECTORIES.CPP).
 * Tests here only assert `lba2.hqr` presence as the discovery gate.
 */

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LIMITS.H>

#include "RES_DISCOVERY.H"

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <cerrno>
#include <cstdlib>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" const U32 g_embeddedLba2CfgBytesSize;
extern "C" int WriteEmbeddedDefaultLba2Cfg(const char *destPath);

#ifdef _WIN32

static int mkdir_portable(const char *p) {
    return _mkdir(p);
}

static int setenv_portable(const char *k, const char *v) {
    return _putenv_s(k, v) == 0 ? 0 : -1;
}

static void unsetenv_portable(const char *k) {
    SetEnvironmentVariableA(k, NULL);
}

static int unlink_portable(const char *p) {
    return _unlink(p);
}

static int rmdir_portable(const char *p) {
    return _rmdir(p);
}

static char *getcwd_portable(char *buf, size_t sz) {
    return _getcwd(buf, static_cast<int>(sz));
}

static int chdir_portable(const char *p) {
    return _chdir(p);
}

/** Creates a unique directory under %TEMP%; `tag` is a short label for the path prefix. */
static bool make_temp_dir(char *out, size_t out_sz, const char *tag) {
    char base[MAX_PATH];
    if (GetTempPathA(sizeof(base), base) == 0) {
        return false;
    }
    for (int i = 0; i < 256; ++i) {
        snprintf(out, out_sz, "%slba2disc_%s_%lu_%d", base, tag, (unsigned long)GetCurrentProcessId(), i);
        if (_mkdir(out) == 0) {
            return true;
        }
        if (errno != EEXIST) {
            return false;
        }
    }
    return false;
}

#else

static int mkdir_portable(const char *p) {
    return mkdir(p, 0755);
}

static int setenv_portable(const char *k, const char *v) {
    return setenv(k, v, 1);
}

static void unsetenv_portable(const char *k) {
    unsetenv(k);
}

static int unlink_portable(const char *p) {
    return unlink(p);
}

static int rmdir_portable(const char *p) {
    return rmdir(p);
}

static char *getcwd_portable(char *buf, size_t sz) {
    return getcwd(buf, sz);
}

static int chdir_portable(const char *p) {
    return chdir(p);
}

#endif

static void create_marker_hqr(const char *dir) {
    char marker[512];
    snprintf(marker, sizeof(marker), "%s/lba2.hqr", dir);
    FILE *f = fopen(marker, "wb");
    if (f) {
        fclose(f);
    }
}

static bool test_env_lba2_game_dir() {
    unsetenv_portable("LBA2_GAME_DIR");
#ifndef _WIN32
    char tmpl[] = "/tmp/lba2disc_ev_XXXXXX";
    if (mkdtemp(tmpl) == NULL) {
        return false;
    }
    const char *const tmpl_ptr = tmpl;
#else
    char tmpl[512];
    if (!make_temp_dir(tmpl, sizeof(tmpl), "ev")) {
        return false;
    }
    const char *const tmpl_ptr = tmpl;
#endif
    create_marker_hqr(tmpl_ptr);

    if (setenv_portable("LBA2_GAME_DIR", tmpl_ptr) != 0) {
        return false;
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};

    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    unsetenv_portable("LBA2_GAME_DIR");

    if (!ok) {
        return false;
    }
    return strstr(out, tmpl_ptr) != NULL;
}

static bool test_argv_game_dir() {
    unsetenv_portable("LBA2_GAME_DIR");
#ifndef _WIN32
    char tmpl[] = "/tmp/lba2disc_arg_XXXXXX";
    if (mkdtemp(tmpl) == NULL) {
        return false;
    }
    const char *const tmpl_ptr = tmpl;
#else
    char tmpl[512];
    if (!make_temp_dir(tmpl, sizeof(tmpl), "arg")) {
        return false;
    }
    const char *const tmpl_ptr = tmpl;
#endif
    create_marker_hqr(tmpl_ptr);

    char out[ADELINE_MAX_PATH];
    char arg0[] = "lba2";
    char arg1[] = "--game-dir";
    char arg2[512];
    strncpy(arg2, tmpl_ptr, sizeof(arg2));
    arg2[sizeof(arg2) - 1] = '\0';
    char *argv[] = {arg0, arg1, arg2, NULL};
    int argc = 3;

    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    if (!ok || argc != 1) {
        return false;
    }
    return strstr(out, tmpl_ptr) != NULL;
}

/**
 * Simulates: clone at parent/repo_clone, retail at parent/RetailGame/lba2.hqr.
 * Discovery scans siblings of parent(repo_clone) and finds RetailGame.
 */
static bool test_sibling_direct_next_to_cwd() {
    unsetenv_portable("LBA2_GAME_DIR");
#ifndef _WIN32
    char parent[] = "/tmp/lba2sibdir_XXXXXX";
    if (mkdtemp(parent) == NULL) {
        return false;
    }
#else
    char parent[512];
    if (!make_temp_dir(parent, sizeof(parent), "sib")) {
        return false;
    }
#endif
    char path[512];
    snprintf(path, sizeof(path), "%s/repo_clone", parent);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/RetailGame", parent);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    create_marker_hqr(path);

    char oldcwd[4096];
    if (getcwd_portable(oldcwd, sizeof(oldcwd)) == NULL) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/repo_clone", parent);
    if (chdir_portable(path) != 0) {
        return false;
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    chdir_portable(oldcwd);
    if (!ok) {
        return false;
    }
    return strstr(out, "RetailGame") != NULL;
}

/**
 * Simulates distributor layout: parent/OddName/CommonClassic/lba2.hqr next to
 * parent/repo_clone (cwd).
 */
static bool test_sibling_commonclassic_nested() {
    unsetenv_portable("LBA2_GAME_DIR");
#ifndef _WIN32
    char parent[] = "/tmp/lba2sibcc_XXXXXX";
    if (mkdtemp(parent) == NULL) {
        return false;
    }
#else
    char parent[512];
    if (!make_temp_dir(parent, sizeof(parent), "scc")) {
        return false;
    }
#endif
    char path[512];
    snprintf(path, sizeof(path), "%s/repo_clone", parent);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/OddName", parent);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/OddName/CommonClassic", parent);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    create_marker_hqr(path);

    char oldcwd[4096];
    if (getcwd_portable(oldcwd, sizeof(oldcwd)) == NULL) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/repo_clone", parent);
    if (chdir_portable(path) != 0) {
        return false;
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    chdir_portable(oldcwd);
    if (!ok) {
        return false;
    }
    return strstr(out, "CommonClassic") != NULL;
}

static bool test_embedded_cfg_write() {
#ifndef _WIN32
    char dir[] = "/tmp/lba2emb_XXXXXX";
    if (mkdtemp(dir) == NULL) {
        return false;
    }
#else
    char dir[512];
    if (!make_temp_dir(dir, sizeof(dir), "emb")) {
        return false;
    }
#endif
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/out.cfg", dir);

    if (!WriteEmbeddedDefaultLba2Cfg(dest)) {
        return false;
    }
    const U32 sz = FileSize(dest);
    unlink_portable(dest);
    rmdir_portable(dir);
    return sz == g_embeddedLba2CfgBytesSize;
}

/* Persisted-LastGameDir probe: a previous picker session wrote
 * last_game_dir.txt to <SDL_GetPrefPath>; ResolveGameDataDir must
 * read it back and return that path before falling through to
 * auto-discovery.
 *
 * Linux-only: SDL_GetPrefPath honors XDG_DATA_HOME on Linux, which
 * lets us redirect to a test-scratch directory cleanly. macOS / Windows
 * use platform-specific paths without an env override; we skip there
 * (still validated manually + via the picker UI flow). */
static bool test_persisted_last_game_dir() {
#ifndef __linux__
    fprintf(stderr, "[skip] test_persisted_last_game_dir: Linux-only\n");
    return true;
#else
    unsetenv_portable("LBA2_GAME_DIR");

    /* Redirect SDL_GetPrefPath via XDG_DATA_HOME. SDL3 reads it on
     * each call (no caching), so setting it before WritePersistedGameDir
     * lands the file in our scratch dir. */
    char xdg[] = "/tmp/lba2disc_xdg_XXXXXX";
    if (mkdtemp(xdg) == NULL) {
        return false;
    }
    if (setenv_portable("XDG_DATA_HOME", xdg) != 0) {
        return false;
    }

    /* Create a valid game-data directory the persisted file will point at. */
    char gameDir[] = "/tmp/lba2disc_pgd_XXXXXX";
    if (mkdtemp(gameDir) == NULL) {
        return false;
    }
    create_marker_hqr(gameDir);

    /* Persist it via the public API. SDL_GetPrefPath now resolves under
     * the scratch XDG_DATA_HOME, so last_game_dir.txt lands there. */
    if (!WritePersistedGameDir(gameDir)) {
        return false;
    }

    /* Run discovery from a directory where auto-discovery would NOT
     * find a valid resource dir. The persisted probe should fire and
     * return our gameDir. */
    char neutralCwd[] = "/tmp/lba2disc_cwd_XXXXXX";
    if (mkdtemp(neutralCwd) == NULL) {
        return false;
    }
    char originalCwd[ADELINE_MAX_PATH];
    getcwd_portable(originalCwd, sizeof(originalCwd));
    chdir_portable(neutralCwd);

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);

    chdir_portable(originalCwd);
    unsetenv_portable("XDG_DATA_HOME");
    /* Best-effort cleanup of scratch dirs. */
    rmdir_portable(neutralCwd);

    if (!ok) {
        fprintf(stderr, "test_persisted_last_game_dir: discovery failed\n");
        return false;
    }
    if (strstr(out, gameDir) == NULL) {
        fprintf(stderr,
                "test_persisted_last_game_dir: got %s, expected to contain %s\n",
                out, gameDir);
        return false;
    }
    return true;
#endif
}

int main() {
    if (!SDL_Init(0)) {
        return 1;
    }
    int failed = 0;
    if (!test_sibling_direct_next_to_cwd()) {
        failed++;
    }
    if (!test_sibling_commonclassic_nested()) {
        failed++;
    }
    if (!test_env_lba2_game_dir()) {
        failed++;
    }
    if (!test_argv_game_dir()) {
        failed++;
    }
    if (!test_embedded_cfg_write()) {
        failed++;
    }
    if (!test_persisted_last_game_dir()) {
        failed++;
    }
    SDL_Quit();
    return failed ? 1 : 0;
}
