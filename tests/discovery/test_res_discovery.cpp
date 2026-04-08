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
    SDL_Quit();
    return failed ? 1 : 0;
}
