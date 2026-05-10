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

/* Backup/restore the user's persisted last_game_dir.txt around the test
 * run. The persisted-LastGameDir probe in ResolveGameDataDir fires
 * BEFORE auto-discovery, so without clearing the persisted file the
 * sibling-scan tests pick up the real user's setting instead of their
 * synthetic test fixture and fail. Backup-and-restore is robust to
 * the user actually having a persisted picker pick on their dev box. */
static char saved_persisted[ADELINE_MAX_PATH];
static bool had_persisted = false;
static char persisted_path[ADELINE_MAX_PATH];

static void compute_persisted_path() {
    char *prefPath = SDL_GetPrefPath("Twinsen", "LBA2");
    if (prefPath == NULL) {
        persisted_path[0] = '\0';
        return;
    }
    snprintf(persisted_path, sizeof(persisted_path), "%slast_game_dir.txt",
             prefPath);
    SDL_free(prefPath);
}

static void backup_persisted_game_dir() {
    compute_persisted_path();
    if (persisted_path[0] == '\0') {
        return;
    }
    FILE *f = fopen(persisted_path, "r");
    if (f != NULL) {
        if (fgets(saved_persisted, sizeof(saved_persisted), f) != NULL) {
            had_persisted = true;
        }
        fclose(f);
        unlink_portable(persisted_path);
    }
}

static void restore_persisted_game_dir() {
    if (!had_persisted || persisted_path[0] == '\0') {
        return;
    }
    FILE *f = fopen(persisted_path, "w");
    if (f != NULL) {
        fputs(saved_persisted, f);
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
 * The challenge: SDL_GetPrefPath caches the resolved path on first
 * call within a process (verified empirically: setenv("XDG_DATA_HOME")
 * after SDL_Init has no effect on subsequent SDL_GetPrefPath calls).
 * So we can't simply override the env var inside the test and expect
 * SDL3 to follow.
 *
 * Approach: try the override, then check whether SDL_GetPrefPath
 * actually picked it up by comparing before/after. If it did → run
 * the full test. If it didn't (SDL cached, override took no effect),
 * skip cleanly without polluting the user's real prefs directory.
 *
 * Linux-only: SDL_GetPrefPath honors XDG_DATA_HOME on Linux when
 * read fresh. macOS / Windows use platform-specific paths without
 * a clean env override; skip there. */
static bool test_persisted_last_game_dir() {
#ifndef __linux__
    fprintf(stderr, "[skip] test_persisted_last_game_dir: Linux-only\n");
    return true;
#else
    unsetenv_portable("LBA2_GAME_DIR");

    /* Snapshot the un-overridden pref path. */
    char *originalPrefPath = SDL_GetPrefPath("Twinsen", "LBA2");
    if (originalPrefPath == NULL) {
        return false;
    }

    char xdg[] = "/tmp/lba2disc_xdg_XXXXXX";
    if (mkdtemp(xdg) == NULL) {
        SDL_free(originalPrefPath);
        return false;
    }
    if (setenv_portable("XDG_DATA_HOME", xdg) != 0) {
        SDL_free(originalPrefPath);
        return false;
    }

    /* Did the env override actually take? SDL3 may have cached the
     * pref path during an earlier call (e.g. from another test in
     * this same process), in which case our setenv is a no-op. */
    char *overriddenPrefPath = SDL_GetPrefPath("Twinsen", "LBA2");
    if (overriddenPrefPath == NULL) {
        SDL_free(originalPrefPath);
        unsetenv_portable("XDG_DATA_HOME");
        return false;
    }
    const bool overrideTook =
        (strstr(overriddenPrefPath, xdg) != NULL);
    SDL_free(originalPrefPath);
    SDL_free(overriddenPrefPath);

    if (!overrideTook) {
        /* SDL_GetPrefPath cached the un-overridden path. Running the
         * full test now would write last_game_dir.txt into the user's
         * real prefs directory and risk clobbering an actual setting.
         * Skip without writing anything. */
        unsetenv_portable("XDG_DATA_HOME");
        fprintf(stderr,
                "[skip] test_persisted_last_game_dir: SDL_GetPrefPath "
                "cached pre-override path; can't isolate.\n");
        return true;
    }

    /* Override took. Safe to write under the scratch XDG_DATA_HOME. */
    char gameDir[] = "/tmp/lba2disc_pgd_XXXXXX";
    if (mkdtemp(gameDir) == NULL) {
        unsetenv_portable("XDG_DATA_HOME");
        return false;
    }
    create_marker_hqr(gameDir);

    if (!WritePersistedGameDir(gameDir)) {
        unsetenv_portable("XDG_DATA_HOME");
        return false;
    }

    /* Run discovery from a directory where auto-discovery would NOT
     * find a valid resource dir. The persisted probe should fire. */
    char neutralCwd[] = "/tmp/lba2disc_cwd_XXXXXX";
    if (mkdtemp(neutralCwd) == NULL) {
        unsetenv_portable("XDG_DATA_HOME");
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

    /* Move the user's real persisted last_game_dir.txt aside (if any)
     * so it doesn't interfere with the sibling-scan tests, which
     * expect auto-discovery to find their synthetic fixtures. Restore
     * via atexit so the user's setting survives test crashes too. */
    backup_persisted_game_dir();
    atexit(restore_persisted_game_dir);

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
