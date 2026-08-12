/**
 * Host-only tests for game data path resolution (no Docker / no ASM).
 *
 * Asset root: the resolved directory is the single `directoriesResDir`; all
 * HQR/music/video paths are relative to it (see GetResPath in DIRECTORIES.CPP).
 * Tests here only assert `lba2.hqr` presence as the discovery gate.
 */

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LIMITS.H>

#include "DIRECTORIES.H"
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

/* People point LBA2_GAME_DIR at the install root, same as --game-dir, so it
 * gets the same Common/ join. */
static bool test_env_lba2_game_dir_common_join() {
    unsetenv_portable("LBA2_GAME_DIR");
#ifndef _WIN32
    char root[] = "/tmp/lba2disc_envc_XXXXXX";
    if (mkdtemp(root) == NULL) {
        return false;
    }
#else
    char root[512];
    if (!make_temp_dir(root, sizeof(root), "envc")) {
        return false;
    }
#endif
    char common[ADELINE_MAX_PATH + 16];
    snprintf(common, sizeof(common), "%s/Common", root);
    if (mkdir_portable(common) != 0) {
        return false;
    }
    create_marker_hqr(common);

    if (setenv_portable("LBA2_GAME_DIR", root) != 0) {
        return false;
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    unsetenv_portable("LBA2_GAME_DIR");

    if (!ok) {
        fprintf(stderr, "test_env_lba2_game_dir_common_join: did not resolve\n");
        return false;
    }
    if (strstr(out, "Common") == NULL) {
        fprintf(stderr,
                "test_env_lba2_game_dir_common_join: got %s, wanted the Common/ inside %s\n",
                out, root);
        return false;
    }
    return true;
}

/* A stale LBA2_GAME_DIR must stop the run, not fall through to auto-discovery.
 * Falling through boots whatever else is lying around: right-looking launch,
 * assets nobody asked for, exit 0. The caller turns this false into its picker,
 * or a clean error when headless. */
static bool test_env_lba2_game_dir_no_fallthrough() {
    unsetenv_portable("LBA2_GAME_DIR");
    if (setenv_portable("LBA2_GAME_DIR", "/nonexistent/lba2disc_stale") != 0) {
        return false;
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    out[0] = '\0';
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    unsetenv_portable("LBA2_GAME_DIR");

    if (ok) {
        fprintf(stderr,
                "test_env_lba2_game_dir_no_fallthrough: resolved %s from a bad env var\n",
                out);
        return false;
    }
    return true;
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

/* ── An installed file beats a mounted image's copy of it (#461) ────────── */

/* Minimal flat ISO9660: a root directory holding LBA2.HQR and LBA2.CFG. Same
 * record encoding as tests/disc_image, without the nested volume directory,
 * which this test does not need. */
static void iso_both32(unsigned char *p, U32 v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
    p[4] = (unsigned char)((v >> 24) & 0xFF);
    p[5] = (unsigned char)((v >> 16) & 0xFF);
    p[6] = (unsigned char)((v >> 8) & 0xFF);
    p[7] = (unsigned char)(v & 0xFF);
}

static void iso_both16(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 8) & 0xFF);
    p[3] = (unsigned char)(v & 0xFF);
}

static void iso_add_rec(unsigned char *sector, int *offset, U32 lba, U32 len,
                        bool isDir, const unsigned char *name, int nameLen) {
    int recLen = 33 + nameLen;
    if (recLen & 1) {
        recLen++;
    }
    unsigned char *r = sector + *offset;
    memset(r, 0, (size_t)recLen);
    r[0] = (unsigned char)recLen;
    iso_both32(r + 2, lba);
    iso_both32(r + 10, len);
    r[25] = isDir ? 0x02 : 0x00;
    iso_both16(r + 28, 1);
    r[32] = (unsigned char)nameLen;
    memcpy(r + 33, name, (size_t)nameLen);
    *offset += recLen;
}

#define ISO_SECTORS 22
#define ISO_L_PVD 16
#define ISO_L_ROOT 18
#define ISO_L_HQR 19
#define ISO_L_CFG 20

static const char kImageCfgBody[] = "FROM-IMAGE";
static const char kDiskCfgBody[] = "FROM-DISK";

static bool write_case_fixture_iso(const char *path) {
    static unsigned char image[ISO_SECTORS * 2048];
    memset(image, 0, sizeof(image));

    unsigned char *pvd = image + ISO_L_PVD * 2048;
    pvd[0] = 0x01;
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 0x01;
    const unsigned char dot = 0x00, dotdot = 0x01;
    int o = 156; /* the root directory record lives at offset 156 of the PVD */
    iso_add_rec(pvd, &o, ISO_L_ROOT, 2048, true, &dot, 1);

    unsigned char *root = image + ISO_L_ROOT * 2048;
    o = 0;
    iso_add_rec(root, &o, ISO_L_ROOT, 2048, true, &dot, 1);
    iso_add_rec(root, &o, ISO_L_ROOT, 2048, true, &dotdot, 1);
    iso_add_rec(root, &o, ISO_L_HQR, 8, false, (const unsigned char *)"LBA2.HQR;1", 10);
    iso_add_rec(root, &o, ISO_L_CFG, (U32)strlen(kImageCfgBody), false,
                (const unsigned char *)"LBA2.CFG;1", 10);

    memcpy(image + ISO_L_HQR * 2048, "HQRSTUB", 7);
    memcpy(image + ISO_L_CFG * 2048, kImageCfgBody, strlen(kImageCfgBody));

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    const bool ok = fwrite(image, 1, sizeof(image), f) == sizeof(image);
    fclose(f);
    return ok;
}

static bool read_whole_file(const char *path, char *out, int outMax) {
    S32 h = OpenRead(path);
    if (!h) {
        return false;
    }
    S32 n = Read(h, out, (U32)(outMax - 1));
    Close(h);
    if (n < 0) {
        return false;
    }
    out[n] = '\0';
    return true;
}

/* A mounted image answers case-insensitively, so a case sweep that lets it
 * speak too early settles on a spelling only the image holds -- and OpenRead,
 * finding nothing on disk under that spelling, then serves the image's copy
 * even though the real file is right there. That is how a GOG install (asked
 * for lba2.cfg, LBA2.CFG on disk, LBA2.GOG mounted) read the image's config
 * instead of its own and came up in French. */
static bool test_installed_file_beats_image_copy() {
    char dir[ADELINE_MAX_PATH];
#ifdef _WIN32
    if (!make_temp_dir(dir, sizeof(dir), "shadow")) {
        return false;
    }
#else
    snprintf(dir, sizeof(dir), "/tmp/lba2disc_shadow_XXXXXX");
    if (mkdtemp(dir) == NULL) {
        return false;
    }
#endif

    char base[ADELINE_MAX_PATH + 8];
    char isoPath[ADELINE_MAX_PATH + 32];
    char diskCfg[ADELINE_MAX_PATH + 32];
    snprintf(base, sizeof(base), "%s/", dir);
    snprintf(isoPath, sizeof(isoPath), "%sdisc.iso", base);
    snprintf(diskCfg, sizeof(diskCfg), "%sLBA2.CFG", base);

    create_marker_hqr(dir);
    if (!write_case_fixture_iso(isoPath)) {
        fprintf(stderr, "test_installed_file_beats_image_copy: cannot write iso\n");
        return false;
    }
    FILE *f = fopen(diskCfg, "wb");
    if (f == NULL) {
        return false;
    }
    fwrite(kDiskCfgBody, 1, strlen(kDiskCfgBody), f);
    fclose(f);

    InitDirectories(base, base, base, "", 0);
    if (!DiscImage_Mount(base, Directories_GetResMarker())) {
        fprintf(stderr, "test_installed_file_beats_image_copy: mount failed\n");
        return false;
    }

    bool ok = true;
    char resolved[ADELINE_MAX_PATH];
    char body[64];

    /* Installed on disk as LBA2.CFG, asked for as lba2.cfg: the installed file
     * wins, both in the resolved spelling and in what comes back. */
    GetDefaultCfgPath(resolved, ADELINE_MAX_PATH, "lba2.cfg");
    if (!read_whole_file(resolved, body, (int)sizeof(body)) ||
        strcmp(body, kDiskCfgBody) != 0) {
        fprintf(stderr,
                "test_installed_file_beats_image_copy: %s served '%s', wanted '%s'\n",
                resolved, body, kDiskCfgBody);
        ok = false;
    }

    /* Nothing installed under any spelling: the image is then the right answer,
     * which is how a bare disc rip (no extracted files at all) still boots. */
    unlink_portable(diskCfg);
    GetDefaultCfgPath(resolved, ADELINE_MAX_PATH, "lba2.cfg");
    if (!read_whole_file(resolved, body, (int)sizeof(body)) ||
        strcmp(body, kImageCfgBody) != 0) {
        fprintf(stderr,
                "test_installed_file_beats_image_copy: image fallback served '%s', wanted '%s'\n",
                body, kImageCfgBody);
        ok = false;
    }

    DiscImage_Unmount();
    return ok;
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
    if (!test_env_lba2_game_dir_common_join()) {
        failed++;
    }
    if (!test_env_lba2_game_dir_no_fallthrough()) {
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
    /* Last: InitDirectories asserts it runs once, and the cases above resolve
     * paths without it. */
    if (!test_installed_file_beats_image_copy()) {
        failed++;
    }
    SDL_Quit();
    return failed ? 1 : 0;
}
