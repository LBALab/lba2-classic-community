/**
 * Host-only tests for game data path resolution (no Docker / no ASM).
 *
 * Asset root: the resolved directory is the single `directoriesResDir`; all
 * HQR/music/video paths are relative to it (see GetResPath in DIRECTORIES.CPP).
 * Tests here only assert `lba2.hqr` presence as the discovery gate.
 */

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/LOG.H>

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
    /* Both halves, deliberately. SetEnvironmentVariableA clears the Win32
       block; the CRT keeps its own copy, and getenv reads that one, so
       clearing only the block leaves the variable visible to the code under
       test. _putenv_s with an empty value removes it from the CRT copy. */
    _putenv_s(k, "");
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

/** Creates a unique directory under /tmp; `tag` is a short label for the path
 *  prefix. Twin of the Windows one above, so a fixture needs no #ifdef of its
 *  own. The older tests predate it and open-code mkdtemp. */
static bool make_temp_dir(char *out, size_t out_sz, const char *tag) {
    if (snprintf(out, out_sz, "/tmp/lba2disc_%s_XXXXXX", tag) >= (int)out_sz) {
        return false;
    }
    return mkdtemp(out) != NULL;
}

#endif

/** Reads a whole file into `buf`, NUL-terminated; empty string if unreadable. */
static void slurp_file(const char *path, char *buf, size_t cap) {
    buf[0] = '\0';
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return;
    }
    const size_t n = fread(buf, 1, cap - 1, f);
    buf[n] = '\0';
    fclose(f);
}

static void create_marker_hqr(const char *dir) {
    char marker[512];
    /* The engine's own marker, not a literal: a demo build gates on RESS.HQR,
       and a fixture spelling the retail name would build a tree that build
       cannot discover. */
    snprintf(marker, sizeof(marker), "%s/%s", dir, Directories_GetResMarker());
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
    char *prefPath = SDL_GetPrefPath(ADELINE_PREF_ORG, ADELINE_PREF_APP);
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
    /* The banner has to name the probe that won, and this is the case that
     * catches a mislabel: the env branch makes two attempts, so a label set
     * once for the pair would report the wrong one for whichever hit second. */
    /* A named profile binds to what the user typed, so the flag that gates that
     * has to be true here and false for the probes below. */
    if (!Res_ResolvedFromUserInput()) {
        fprintf(stderr,
                "test_env_lba2_game_dir_common_join: LBA2_GAME_DIR did not count as user input\n");
        return false;
    }
    const char *via = Res_GetDiscoverySource();
    if (strcmp(via, "LBA2_GAME_DIR, Common/") != 0) {
        fprintf(stderr,
                "test_env_lba2_game_dir_common_join: labelled '%s', wanted 'LBA2_GAME_DIR, Common/'\n",
                via);
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
    if (strstr(out, tmpl_ptr) == NULL) {
        return false;
    }
    const char *via = Res_GetDiscoverySource();
    if (strcmp(via, "--game-dir") != 0) {
        fprintf(stderr, "test_argv_game_dir: labelled '%s', wanted '--game-dir'\n", via);
        return false;
    }
    return true;
}

/**
 * Simulates: clone at parent/repo_clone, retail at parent/RetailGame/lba2.hqr.
 * Discovery scans siblings of parent(repo_clone) and finds RetailGame.
 *
 * Also the substitution case: the run boots an install the caller never named,
 * picked by enumerating the parent. That is wanted here and unwanted when the
 * folder you are standing in is itself an install this build cannot read (a
 * demo beside a retail copy), and discovery cannot tell those apart. So it
 * stays a fallback and has to announce itself; this asserts it does.
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

    /* File sinks flush per write, so the warning can be read back without
       Log_Shutdown tearing logging down for every test that runs after.
       Log_Init is what routes records to sinks at all; until it runs they go
       to SDL's default stderr and no sink sees them. */
    char logPath[600];
    snprintf(logPath, sizeof(logPath), "%s/sibling.log", parent);
    LogSink *fileSink = Log_MakeFileSink(logPath, LOG_WARN);
    Log_AddSink(fileSink);

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    chdir_portable(oldcwd);
    if (!ok) {
        return false;
    }
    if (strstr(out, "RetailGame") == NULL) {
        fprintf(stderr, "test_sibling_direct_next_to_cwd: resolved '%s'\n", out);
        return false;
    }

    char logBuf[4096];
    slurp_file(logPath, logBuf, sizeof(logBuf));
    Log_RemoveSink(fileSink);
    if (strstr(logBuf, "neighbouring install") == NULL) {
        fprintf(stderr,
                "test_sibling_direct_next_to_cwd: booted an install the caller "
                "never named without saying so; log was:\n%s\n",
                logBuf);
        return false;
    }
    return true;
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

/**
 * Steam-library regression: cwd is the install, HQRs are in its Common/, and
 * the parent holds more folders than the sibling scan's kMaxSiblingEntries
 * cap. Reported from a Deck with 25+ games in steamapps/common/, where the
 * install the user was standing in fell off the end of the scan in readdir
 * order and discovery gave up after 138 candidates.
 *
 * The cwd Common/ join has to be what answers this, so assert the discovery
 * source and not just the result: the sibling scan can also reach
 * <cwd>/Common/ from the parent, and whether it does depends on readdir
 * order, which is exactly the non-determinism this test exists to rule out.
 */
static bool test_cwd_common_join_beats_sibling_cap() {
    unsetenv_portable("LBA2_GAME_DIR");
#ifndef _WIN32
    char parent[] = "/tmp/lba2cwdcommon_XXXXXX";
    if (mkdtemp(parent) == NULL) {
        return false;
    }
#else
    char parent[512];
    if (!make_temp_dir(parent, sizeof(parent), "cwdc")) {
        return false;
    }
#endif
    char path[512];

    /* Decoys: enough to exceed kMaxSiblingEntries (24). None of them hold
       game data, so any of them matching would be a bug in its own right. */
    for (int i = 0; i < 30; i++) {
        snprintf(path, sizeof(path), "%s/Decoy Game %02d", parent, i);
        if (mkdir_portable(path) != 0) {
            return false;
        }
    }

    snprintf(path, sizeof(path), "%s/Little Big Adventure 2", parent);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/Little Big Adventure 2/Common", parent);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    create_marker_hqr(path);

    char oldcwd[4096];
    if (getcwd_portable(oldcwd, sizeof(oldcwd)) == NULL) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/Little Big Adventure 2", parent);
    if (chdir_portable(path) != 0) {
        return false;
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    const char *source = Res_GetDiscoverySource();
    char sourceCopy[128];
    snprintf(sourceCopy, sizeof(sourceCopy), "%s", source != NULL ? source : "");
    chdir_portable(oldcwd);

    if (!ok) {
        fprintf(stderr,
                "test_cwd_common_join_beats_sibling_cap: discovery failed to "
                "find Common/ in the working directory\n");
        return false;
    }
    if (strstr(out, "Common") == NULL) {
        fprintf(stderr,
                "test_cwd_common_join_beats_sibling_cap: resolved '%s', "
                "expected the Common/ subdirectory\n",
                out);
        return false;
    }
    if (strcmp(sourceCopy, "working directory, Common/") != 0) {
        fprintf(stderr,
                "test_cwd_common_join_beats_sibling_cap: matched via '%s', "
                "expected 'working directory, Common/'\n",
                sourceCopy);
        return false;
    }
    return true;
}

/**
 * Common/ outranks the data/ and game/ joins under the same root. A folder
 * holding two valid installs is ambiguous either way, so this pins the answer
 * rather than leaving it to the order the probes happen to be written in:
 * Common/ is the layout a retail install actually ships, data/ and game/ are
 * conventions people built by hand.
 */
static bool test_cwd_common_outranks_data() {
    unsetenv_portable("LBA2_GAME_DIR");
#ifndef _WIN32
    char root[] = "/tmp/lba2prec_XXXXXX";
    if (mkdtemp(root) == NULL) {
        return false;
    }
#else
    char root[512];
    if (!make_temp_dir(root, sizeof(root), "prec")) {
        return false;
    }
#endif
    char path[512];
    snprintf(path, sizeof(path), "%s/install", root);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/install/Common", root);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    create_marker_hqr(path);
    snprintf(path, sizeof(path), "%s/install/data", root);
    if (mkdir_portable(path) != 0) {
        return false;
    }
    create_marker_hqr(path);

    char oldcwd[4096];
    if (getcwd_portable(oldcwd, sizeof(oldcwd)) == NULL) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/install", root);
    if (chdir_portable(path) != 0) {
        return false;
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    const char *via = Res_GetDiscoverySource();
    char viaCopy[128];
    snprintf(viaCopy, sizeof(viaCopy), "%s", via != NULL ? via : "");
    chdir_portable(oldcwd);

    if (!ok) {
        fprintf(stderr, "test_cwd_common_outranks_data: nothing resolved\n");
        return false;
    }
    if (strcmp(viaCopy, "working directory, Common/") != 0) {
        fprintf(stderr,
                "test_cwd_common_outranks_data: resolved '%s' via '%s', "
                "expected the Common/ join to win\n",
                out, viaCopy);
        return false;
    }
    return true;
}

/* The sibling scan is the probe that boots an install nobody named. A named
 * profile writes down what resolved it, so this must never report as user
 * input: binding a guess would make an arbitrary folder that profile's install
 * for good. */
static bool test_sibling_scan_is_not_user_input() {
    unsetenv_portable("LBA2_GAME_DIR");
    Res_SetDiscoverySource("sibling scan");
    if (Res_ResolvedFromUserInput()) {
        printf("FAIL provenance: the sibling scan counted as user input\n");
        return false;
    }
    Res_SetDiscoverySource("last_game_dir.txt");
    if (Res_ResolvedFromUserInput()) {
        printf("FAIL provenance: a remembered pick counted as user input\n");
        return false;
    }
    Res_SetDiscoverySource("--game-dir");
    if (!Res_ResolvedFromUserInput()) {
        printf("FAIL provenance: --game-dir did not count as user input\n");
        return false;
    }
    Res_SetDiscoverySource("--game-dir, Common/");
    if (!Res_ResolvedFromUserInput()) {
        printf("FAIL provenance: --game-dir with the Common/ join was missed\n");
        return false;
    }
    return true;
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

/* User-directory precedence: --user-dir beats LBA2_USER_DIR, an empty string
 * counts as nothing set, and whatever wins comes back with exactly one trailing
 * separator. That last part is what keeps a path typed without one from making
 * every Get*Path append a sibling instead of a child.
 *
 * Exercises Directories_ResolveUserDir rather than GetDefaultUserDir: the
 * latter resolves once and caches, so a process can only ask it a single
 * question, which is the same wall the persisted-path test below runs into. */
static bool test_user_dir_precedence() {
    char out[ADELINE_MAX_PATH];
    int bad = 0;

    /* Nothing named: report that, and leave the buffer alone so the caller's
     * platform default is not quietly overwritten with a partial answer. */
    strcpy(out, "untouched");
    if (Directories_ResolveUserDir(out, ADELINE_MAX_PATH, NULL, NULL) ||
        strcmp(out, "untouched") != 0) {
        printf("FAIL user_dir: both absent should resolve nothing\n");
        bad++;
    }
    if (Directories_ResolveUserDir(out, ADELINE_MAX_PATH, "", "")) {
        printf("FAIL user_dir: empty strings should count as absent\n");
        bad++;
    }

    /* Precedence, including an empty override falling through to the env. */
    if (!Directories_ResolveUserDir(out, ADELINE_MAX_PATH, "/cli", "/env") ||
        strncmp(out, "/cli", 4) != 0) {
        printf("FAIL user_dir: command line should beat the environment\n");
        bad++;
    }
    if (!Directories_ResolveUserDir(out, ADELINE_MAX_PATH, "", "/env") ||
        strncmp(out, "/env", 4) != 0) {
        printf("FAIL user_dir: empty override should fall through to the environment\n");
        bad++;
    }
    if (!Directories_ResolveUserDir(out, ADELINE_MAX_PATH, NULL, "/env") ||
        strncmp(out, "/env", 4) != 0) {
        printf("FAIL user_dir: null override should fall through to the environment\n");
        bad++;
    }

    /* Exactly one trailing separator, whether or not one was typed, and
     * whichever of the two spellings this platform accepts. */
    char want[ADELINE_MAX_PATH];
    snprintf(want, sizeof want, "/a/b%c", ADELINE_PATH_SEP_CHAR);
    if (!Directories_ResolveUserDir(out, ADELINE_MAX_PATH, "/a/b", NULL) ||
        strcmp(out, want) != 0) {
        printf("FAIL user_dir: missing separator should be appended\n");
        bad++;
    }
    if (!Directories_ResolveUserDir(out, ADELINE_MAX_PATH, "/a/b/", NULL) ||
        strcmp(out, "/a/b/") != 0) {
        printf("FAIL user_dir: an existing '/' should not be doubled\n");
        bad++;
    }
    if (!Directories_ResolveUserDir(out, ADELINE_MAX_PATH, want, NULL) ||
        strcmp(out, want) != 0) {
        printf("FAIL user_dir: an existing native separator should not be doubled\n");
        bad++;
    }

    /* No room for the path plus its separator: refused outright. Truncating to
     * fit, or returning it without the separator, both name a different
     * directory, and a run that quietly wrote to a different directory is what
     * this whole path exists to prevent. The buffer is left alone. */
    char longPath[ADELINE_MAX_PATH];
    memset(longPath, 'a', ADELINE_MAX_PATH - 1);
    longPath[ADELINE_MAX_PATH - 1] = '\0';
    strcpy(out, "untouched");
    if (Directories_ResolveUserDir(out, ADELINE_MAX_PATH, longPath, NULL) ||
        strcmp(out, "untouched") != 0) {
        printf("FAIL user_dir: a path with no room for its separator should be refused\n");
        bad++;
    }

    /* The longest path that does fit still gets its separator. */
    char fits[ADELINE_MAX_PATH];
    memset(fits, 'b', ADELINE_MAX_PATH - 2);
    fits[ADELINE_MAX_PATH - 2] = '\0';
    if (!Directories_ResolveUserDir(out, ADELINE_MAX_PATH, fits, NULL) ||
        strlen(out) != (size_t)(ADELINE_MAX_PATH - 1) ||
        out[ADELINE_MAX_PATH - 2] != ADELINE_PATH_SEP_CHAR) {
        printf("FAIL user_dir: the longest path that fits should still be separated\n");
        bad++;
    }

    return bad == 0;
}

/* A profile is a directory inside the user directory, so its name must not be
 * able to name anything outside it. Rejection happens before the run resolves
 * any path, because a name that escaped would put the saves somewhere nobody
 * asked for and there is no log yet to say so. */
static bool test_profile_name_rules() {
    int bad = 0;

    /* Ordinary names people will actually type, dots and spaces included. */
    static const char *const good[] = {"gog", "ea-cd", "twinsen", "v1.2",
                                       "my install", "A_B-2", ".hidden"};
    for (size_t i = 0; i < sizeof good / sizeof good[0]; i++) {
        if (!Directories_IsValidProfileName(good[i])) {
            printf("FAIL profile: rejected the usable name '%s'\n", good[i]);
            bad++;
        }
    }

    /* Anything that could step out of the profiles directory, plus nothing at
     * all. Both separators are refused whatever the host, so a name written on
     * one platform is refused on the other rather than quietly meaning
     * something different. ".." is rejected wherever it appears, which also
     * costs a name like "v1..2"; that is the intended trade. */
    static const char *const evil[] = {"", "/", "\\", "a/b", "a\\b", "..",
                                       "../escape", "escape/..", ".", "a..b",
                                       "/etc", "C:\\Windows"};
    for (size_t i = 0; i < sizeof evil / sizeof evil[0]; i++) {
        if (Directories_IsValidProfileName(evil[i])) {
            printf("FAIL profile: accepted the unusable name '%s'\n", evil[i]);
            bad++;
        }
    }

    if (Directories_IsValidProfileName(NULL)) {
        printf("FAIL profile: accepted a null name\n");
        bad++;
    }

    return bad == 0;
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
    char *originalPrefPath = SDL_GetPrefPath(ADELINE_PREF_ORG, ADELINE_PREF_APP);
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
    char *overriddenPrefPath = SDL_GetPrefPath(ADELINE_PREF_ORG, ADELINE_PREF_APP);
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
    /* Same reason as create_marker_hqr: the in-image marker has to be the one
       this build looks for. ISO9660 identifiers carry the ";1" version suffix. */
    char isoMarker[32];
    snprintf(isoMarker, sizeof isoMarker, "%s;1", Directories_GetResMarker());
    iso_add_rec(root, &o, ISO_L_HQR, 8, false, (const unsigned char *)isoMarker,
                (unsigned char)strlen(isoMarker));
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

/* An AppImage sees SDL_GetBasePath as its own read-only mount point, so the
 * only thing naming the folder the player put the file in is $APPIMAGE. These
 * drive that probe.
 *
 * Each one parks the working directory in an empty tree of its own, so cwd,
 * the parent walk and the sibling scan have nothing to find. The assertion is
 * on Res_GetDiscoverySource rather than on the resolved path alone: two probes
 * can reach the same directory, and which one answered is the behaviour.
 *
 * $APPDIR is set alongside $APPIMAGE because the seam requires the running
 * executable to sit inside the mount before it believes either. The test binary
 * is not in an AppImage, so the fixture points $APPDIR at the binary's own
 * folder, which is what being inside one looks like from in here. */

/** Empty dir inside an empty parent, for a working directory that finds
 *  nothing by itself. Both paths are returned so the caller can remove them. */
static bool make_isolated_cwd(char *out, size_t out_sz, char *outParent,
                              size_t outParent_sz, const char *tag) {
    if (!make_temp_dir(outParent, outParent_sz, tag)) {
        return false;
    }
    if (snprintf(out, out_sz, "%s/cwd", outParent) >= (int)out_sz) {
        return false;
    }
    return mkdir_portable(out) == 0;
}

/** Best-effort removal of a fixture root: the marker and any disc image in it,
 *  the same inside a Common/ under it, then the directories. Leaving these
 *  behind matters more than usual here, because /tmp is the parent the sibling
 *  scan enumerates for any test that runs from a directory under it. */
static void remove_fixture_root(const char *root) {
    static const char *const kNames[] = {"LBA2.HQR", "RESS.HQR", "disc.iso"};
    char path[768];

    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
        snprintf(path, sizeof(path), "%s/Common/%s", root, kNames[i]);
        unlink_portable(path);
        snprintf(path, sizeof(path), "%s/%s", root, kNames[i]);
        unlink_portable(path);
    }
    snprintf(path, sizeof(path), "%s/Common", root);
    rmdir_portable(path);
    rmdir_portable(root);
}

/** The mount an AppImage would be running from: the test binary's own folder.
 *  Empty string if SDL cannot say, which fails the fixture rather than the
 *  code under test. */
static void this_executable_dir(char *out, size_t out_sz) {
    const char *base = SDL_GetBasePath();
    snprintf(out, out_sz, "%s", base != NULL ? base : "");
}

/** Runs discovery with $APPIMAGE set to `appImagePath`, $APPDIR to `appDir`,
 *  and the working directory parked somewhere empty. Every output is written
 *  on every path, including the early failures, so a caller that ignores the
 *  return still reads initialized memory. */
static bool resolve_with_appimage(const char *appImagePath, const char *appDir,
                                  const char *tag, char *outDir, size_t outDirSz,
                                  char *outSource, size_t outSourceSz) {
    snprintf(outDir, outDirSz, "%s", "");
    snprintf(outSource, outSourceSz, "%s", "");

    char isolated[512];
    char isolatedParent[256];
    if (!make_isolated_cwd(isolated, sizeof(isolated), isolatedParent,
                           sizeof(isolatedParent), tag)) {
        return false;
    }
    char oldcwd[4096];
    if (getcwd_portable(oldcwd, sizeof(oldcwd)) == NULL) {
        return false;
    }
    if (chdir_portable(isolated) != 0) {
        return false;
    }

    unsetenv_portable("LBA2_GAME_DIR");
    setenv_portable("APPIMAGE", appImagePath);
    if (appDir != NULL) {
        setenv_portable("APPDIR", appDir);
    } else {
        unsetenv_portable("APPDIR");
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};
    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    const char *source = Res_GetDiscoverySource();
    snprintf(outSource, outSourceSz, "%s", source != NULL ? source : "");
    snprintf(outDir, outDirSz, "%s", out);

    unsetenv_portable("APPIMAGE");
    unsetenv_portable("APPDIR");
    chdir_portable(oldcwd);
    rmdir_portable(isolated);
    rmdir_portable(isolatedParent);
    return ok;
}

/** The Steam layout: HQRs in Common/, AppImage dropped in the install root.
 *  This is the reported Steam Deck case with the working directory not
 *  helping, which is what a file manager launch looks like. */
static bool test_appimage_dir_common_join() {
    char root[512];
    if (!make_temp_dir(root, sizeof(root), "aicom")) {
        return false;
    }
    char common[640];
    snprintf(common, sizeof(common), "%s/Common", root);
    if (mkdir_portable(common) != 0) {
        return false;
    }
    create_marker_hqr(common);

    char appImage[640];
    snprintf(appImage, sizeof(appImage), "%s/lba2cc-x86_64.AppImage", root);
    char mount[ADELINE_MAX_PATH];
    this_executable_dir(mount, sizeof(mount));

    char out[ADELINE_MAX_PATH];
    char source[128];
    const bool ok = resolve_with_appimage(appImage, mount, "aicomc", out,
                                          sizeof(out), source, sizeof(source));
    remove_fixture_root(root);

    if (!ok) {
        fprintf(stderr,
                "test_appimage_dir_common_join: discovery failed to find "
                "Common/ beside the AppImage\n");
        return false;
    }
    if (strstr(out, "Common") == NULL) {
        fprintf(stderr,
                "test_appimage_dir_common_join: resolved '%s', expected the "
                "Common/ subdirectory\n",
                out);
        return false;
    }
    if (strcmp(source, "next to the AppImage, Common/") != 0) {
        fprintf(stderr,
                "test_appimage_dir_common_join: matched via '%s', expected "
                "'next to the AppImage, Common/'\n",
                source);
        return false;
    }
    return true;
}

/** The GOG and demo layout: HQRs loose in the folder the AppImage sits in. */
static bool test_appimage_dir_bare() {
    char root[512];
    if (!make_temp_dir(root, sizeof(root), "aibare")) {
        return false;
    }
    create_marker_hqr(root);

    char appImage[640];
    snprintf(appImage, sizeof(appImage), "%s/lba2cc-x86_64.AppImage", root);
    char mount[ADELINE_MAX_PATH];
    this_executable_dir(mount, sizeof(mount));

    char out[ADELINE_MAX_PATH];
    char source[128];
    const bool ok = resolve_with_appimage(appImage, mount, "aibarec", out,
                                          sizeof(out), source, sizeof(source));
    remove_fixture_root(root);

    if (!ok) {
        fprintf(stderr,
                "test_appimage_dir_bare: discovery failed on a marker beside "
                "the AppImage\n");
        return false;
    }
    if (strcmp(source, "next to the AppImage") != 0) {
        fprintf(stderr,
                "test_appimage_dir_bare: matched via '%s', expected 'next to "
                "the AppImage'\n",
                source);
        return false;
    }
    return true;
}

/** A rip and the AppImage, nothing extracted. Covers the allowImage widening
 *  on the bare join: dropping the binary next to a disc image is a thing
 *  people do, and the folder holds no HQR of its own to find. */
static bool test_appimage_dir_disc_image() {
    char root[512];
    if (!make_temp_dir(root, sizeof(root), "aiiso")) {
        return false;
    }
    char isoPath[640];
    snprintf(isoPath, sizeof(isoPath), "%s/disc.iso", root);
    if (!write_case_fixture_iso(isoPath)) {
        fprintf(stderr, "test_appimage_dir_disc_image: cannot write iso\n");
        return false;
    }

    char appImage[640];
    snprintf(appImage, sizeof(appImage), "%s/lba2cc-x86_64.AppImage", root);
    char mount[ADELINE_MAX_PATH];
    this_executable_dir(mount, sizeof(mount));

    char out[ADELINE_MAX_PATH];
    char source[128];
    const bool ok = resolve_with_appimage(appImage, mount, "aiisoc", out,
                                          sizeof(out), source, sizeof(source));
    remove_fixture_root(root);

    if (!ok) {
        fprintf(stderr,
                "test_appimage_dir_disc_image: discovery failed on a disc "
                "image beside the AppImage\n");
        return false;
    }
    if (strcmp(source, "next to the AppImage") != 0) {
        fprintf(stderr,
                "test_appimage_dir_disc_image: matched via '%s', expected "
                "'next to the AppImage'\n",
                source);
        return false;
    }
    return true;
}

/**
 * The AppImage's folder outranks the folder the executable is in.
 *
 * This is the whole point of the probe and nothing else pins it: for an
 * AppImage the executable sits in a populated read-only /tmp mount, so a
 * marker there must lose to one beside the launched file. Both are planted and
 * the source label says which won. Moving the probe below the binary's own
 * folder passes every other case in this file and fails this one.
 */
static bool test_appimage_dir_outranks_binary_dir() {
    char root[512];
    if (!make_temp_dir(root, sizeof(root), "airank")) {
        return false;
    }
    create_marker_hqr(root);

    char mount[ADELINE_MAX_PATH];
    this_executable_dir(mount, sizeof(mount));
    if (mount[0] == '\0') {
        fprintf(stderr, "test_appimage_dir_outranks_binary_dir: no base path\n");
        return false;
    }
    /* A marker in the executable's own directory, which is what an AppImage
       mount looks like: a folder full of files that is not where the player
       put anything. Removed again below, however this ends. */
    create_marker_hqr(mount);

    char appImage[640];
    snprintf(appImage, sizeof(appImage), "%s/lba2cc-x86_64.AppImage", root);

    char out[ADELINE_MAX_PATH];
    char source[128];
    const bool ok = resolve_with_appimage(appImage, mount, "airankc", out,
                                          sizeof(out), source, sizeof(source));

    char plantedMarker[768];
    snprintf(plantedMarker, sizeof(plantedMarker), "%s%s", mount,
             Directories_GetResMarker());
    unlink_portable(plantedMarker);
    remove_fixture_root(root);

    if (!ok) {
        fprintf(stderr,
                "test_appimage_dir_outranks_binary_dir: discovery failed\n");
        return false;
    }
    if (strcmp(source, "next to the AppImage") != 0) {
        fprintf(stderr,
                "test_appimage_dir_outranks_binary_dir: matched via '%s', "
                "expected 'next to the AppImage' to outrank the binary's own "
                "folder\n",
                source);
        return false;
    }
    return true;
}

/**
 * $APPIMAGE inherited from an unrelated AppImage is ignored.
 *
 * The runtime exports it into every child process, so an AppImage terminal
 * hands it to a distribution build of this engine launched from that shell.
 * Answering there would point discovery at another application's folder and
 * call it, in the banner, a packaging this build is not. The fixture is the
 * hostile one: the folder DOES hold game data, so a probe that trusts the
 * variable succeeds and reports the wrong mechanism.
 */
static bool test_appimage_outside_appdir_ignored() {
    char root[512];
    if (!make_temp_dir(root, sizeof(root), "aiforeign")) {
        return false;
    }
    create_marker_hqr(root);

    char appImage[640];
    snprintf(appImage, sizeof(appImage), "%s/some-other-app.AppImage", root);

    char out[ADELINE_MAX_PATH];
    char source[128];
    /* $APPDIR names a mount this executable is not inside, which is exactly
       what an inherited environment looks like. */
    resolve_with_appimage(appImage, "/tmp/.mount_someotherapp", "aiforeignc",
                          out, sizeof(out), source, sizeof(source));
    remove_fixture_root(root);

    if (strstr(source, "AppImage") != NULL) {
        fprintf(stderr,
                "test_appimage_outside_appdir_ignored: matched via '%s' on an "
                "$APPIMAGE belonging to another process\n",
                source);
        return false;
    }
    return true;
}

/**
 * A long filename under a short directory still resolves.
 *
 * The refusal is about the answer, which is the directory, not about the value
 * it was derived from. Measuring the whole string instead would drop a folder
 * that fits several times over merely because the file in it has a long name.
 */
static bool test_appimage_long_filename_resolves() {
    char root[512];
    if (!make_temp_dir(root, sizeof(root), "ailongname")) {
        return false;
    }
    create_marker_hqr(root);

    char appImage[ADELINE_MAX_PATH * 2];
    int n = snprintf(appImage, sizeof(appImage), "%s/", root);
    for (int i = n; i < ADELINE_MAX_PATH + 16; i++) {
        appImage[i] = 'a';
    }
    appImage[ADELINE_MAX_PATH + 16] = '\0';

    char mount[ADELINE_MAX_PATH];
    this_executable_dir(mount, sizeof(mount));

    char out[ADELINE_MAX_PATH];
    char source[128];
    const bool ok = resolve_with_appimage(appImage, mount, "ailongnamec", out,
                                          sizeof(out), source, sizeof(source));
    remove_fixture_root(root);

    if (!ok || strcmp(source, "next to the AppImage") != 0) {
        fprintf(stderr,
                "test_appimage_long_filename_resolves: matched via '%s' on a "
                "%d-char $APPIMAGE whose directory is only %d chars\n",
                source, (int)strlen(appImage), (int)strlen(root) + 1);
        return false;
    }
    return true;
}

/**
 * A directory too long to hold is refused, not truncated.
 *
 * Copy-then-strip would cut the value mid-path and remove the remains as the
 * last component, landing on a shorter directory that may well exist, with
 * nothing to tell it apart from the right answer. Pure string handling, so the
 * fixture need not exist on disk.
 */
static bool test_appimage_overlong_dir_ignored() {
    char appImage[ADELINE_MAX_PATH * 2];
    appImage[0] = '/';
    for (int i = 1; i < ADELINE_MAX_PATH + 8; i++) {
        appImage[i] = 'd';
    }
    snprintf(appImage + ADELINE_MAX_PATH + 8,
             sizeof(appImage) - (ADELINE_MAX_PATH + 8), "/lba2cc.AppImage");

    char mount[ADELINE_MAX_PATH];
    this_executable_dir(mount, sizeof(mount));

    char out[ADELINE_MAX_PATH];
    char source[128];
    resolve_with_appimage(appImage, mount, "aioverlong", out, sizeof(out),
                          source, sizeof(source));

    if (strstr(source, "AppImage") != NULL) {
        fprintf(stderr,
                "test_appimage_overlong_dir_ignored: matched via '%s' on a "
                "directory of %d chars; it should have been refused\n",
                source, ADELINE_MAX_PATH + 8);
        return false;
    }
    return true;
}

int main() {
    if (!SDL_Init(0)) {
        return 1;
    }

    /* Route records through the sink layer rather than SDL's default stderr,
       so a test can read back what the engine logged. The terminal sink keeps
       the diagnostics these tests print on failure visible. */
    Log_Init();
    Log_AddSink(Log_MakeTerminalSink(LOG_INFO));

    /* Move the user's real persisted last_game_dir.txt aside (if any)
     * so it doesn't interfere with the sibling-scan tests, which
     * expect auto-discovery to find their synthetic fixtures. Restore
     * via atexit so the user's setting survives test crashes too. */
    backup_persisted_game_dir();
    atexit(restore_persisted_game_dir);

    /* Same reasoning one probe over: discovery now looks at $APPIMAGE before
       the working directory, so a suite run from inside an AppImage would
       resolve to that folder instead of each test's fixture. The AppImage
       cases set both themselves and clear them again. */
    unsetenv_portable("APPIMAGE");
    unsetenv_portable("APPDIR");

    int failed = 0;
    if (!test_sibling_direct_next_to_cwd()) {
        failed++;
    }
    if (!test_sibling_commonclassic_nested()) {
        failed++;
    }
    if (!test_cwd_common_join_beats_sibling_cap()) {
        failed++;
    }
    if (!test_cwd_common_outranks_data()) {
        failed++;
    }
    if (!test_appimage_dir_common_join()) {
        failed++;
    }
    if (!test_appimage_dir_bare()) {
        failed++;
    }
    if (!test_appimage_dir_disc_image()) {
        failed++;
    }
    if (!test_appimage_dir_outranks_binary_dir()) {
        failed++;
    }
    if (!test_appimage_outside_appdir_ignored()) {
        failed++;
    }
    if (!test_appimage_long_filename_resolves()) {
        failed++;
    }
    if (!test_appimage_overlong_dir_ignored()) {
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
    if (!test_user_dir_precedence()) {
        failed++;
    }
    if (!test_profile_name_rules()) {
        failed++;
    }
    if (!test_sibling_scan_is_not_user_input()) {
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
