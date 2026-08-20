/**
 * Host tests for AddDirIfNot, the one place every user-writable folder is made.
 *
 * The last case is why this is a host test and not only a fixture. The first
 * component of an absolute Windows path is a drive spec, and `C:` names the
 * current directory *of drive C*, so creating a folder while the process sits
 * on another drive, or on that drive's root, goes through path handling nothing
 * else here reaches. A process can move itself, so one drive is enough: from the
 * volume root, `C:` resolves to `C:\`, which cannot be created.
 *
 * The fixture beside this (tests/automation/test_user_dirs.sh) cannot stand in
 * for it. It needs retail game data and a display, so no workflow runs it, and
 * the folder it hands the engine comes from mktemp: on the build's own drive,
 * spelt with forward slashes. This runs in ctest -L host_quick, Windows PR job
 * included.
 */

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LIMITS.H>

#include "DIRECTORIES.H"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <cerrno>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

static int g_failures = 0;

static void check(bool cond, const char *what) {
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

static bool is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

#ifdef _WIN32

static bool make_temp_dir(char *out, size_t out_sz) {
    char base[MAX_PATH];
    if (GetTempPathA(sizeof(base), base) == 0) {
        return false;
    }
    for (int i = 0; i < 256; ++i) {
        snprintf(out, out_sz, "%slba2dirs_%lu_%d", base, (unsigned long)GetCurrentProcessId(), i);
        if (_mkdir(out) == 0) {
            return true;
        }
        if (errno != EEXIST) {
            return false;
        }
    }
    return false;
}

static int chdir_portable(const char *p) { return _chdir(p); }
static char *getcwd_portable(char *buf, size_t sz) { return _getcwd(buf, (int)sz); }

/* The root of the drive `path` is on, which is a directory that always exists and
   is never the one being created. `C:` alone would mean "wherever this process is
   on drive C", which is the whole point being tested. */
static void volume_root_of(const char *path, char *out, size_t out_sz) {
    if (path[0] != '\0' && path[1] == ':') {
        snprintf(out, out_sz, "%c:\\", path[0]);
    } else {
        snprintf(out, out_sz, "\\"); /* a UNC path: its own root is the best available */
    }
}

#else

static bool make_temp_dir(char *out, size_t out_sz) {
    if (snprintf(out, out_sz, "/tmp/lba2dirs_XXXXXX") >= (int)out_sz) {
        return false;
    }
    return mkdtemp(out) != NULL;
}

static int chdir_portable(const char *p) { return chdir(p); }
static char *getcwd_portable(char *buf, size_t sz) { return getcwd(buf, sz); }

static void volume_root_of(const char *path, char *out, size_t out_sz) {
    (void)path;
    snprintf(out, out_sz, "/");
}

#endif

/* The platform's own separator, because that is what every caller passes: the
   paths come from GetSavePath and friends, which append with it. */
#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif

int main(void) {
    char root[ADELINE_MAX_PATH];
    if (!make_temp_dir(root, sizeof root)) {
        printf("FAIL: could not make a temp directory to test in\n");
        return 1;
    }

    /* Nested, and with a trailing separator, which is the shape every caller
       passes: the paths come from GetSavePath and friends, which end with one. */
    char nested[ADELINE_MAX_PATH];
    snprintf(nested, sizeof nested, "%s" SEP "save" SEP "shoot" SEP, root);
    check(AddDirIfNot(nested) != 0, "AddDirIfNot reported failure creating a nested path");
    check(is_dir(nested), "the nested path was not created");

    char parent[ADELINE_MAX_PATH];
    snprintf(parent, sizeof parent, "%s" SEP "save", root);
    check(is_dir(parent), "the missing parent was not created");

    /* Idempotent: a folder already there is success, not a failure to report. */
    check(AddDirIfNot(nested) != 0, "AddDirIfNot failed on a directory that already exists");

    /* A name taken by a file is the one failure that never reaches SDL, and the
       caller has to be able to tell it from success. */
    char taken[ADELINE_MAX_PATH];
    snprintf(taken, sizeof taken, "%s" SEP "afile", root);
    FILE *f = fopen(taken, "wb");
    check(f != NULL, "could not write the file that takes the name");
    if (f != NULL) {
        fclose(f);
    }
    check(AddDirIfNot(taken) == 0, "AddDirIfNot claimed a file was a directory");

    /* The case the fixture cannot reach here. From the root of the volume the
       folder is going on, the first component of its absolute path resolves to a
       directory that cannot be created, and a create has to get past that to the
       component that matters. */
    char here[ADELINE_MAX_PATH];
    char volume[ADELINE_MAX_PATH];
    char fromroot[ADELINE_MAX_PATH];
    if (getcwd_portable(here, sizeof here) == NULL) {
        printf("FAIL: could not read the current directory\n");
        return 1;
    }
    volume_root_of(root, volume, sizeof volume);
    snprintf(fromroot, sizeof fromroot, "%s" SEP "recordings" SEP, root);
    check(chdir_portable(volume) == 0, "could not move to the volume root to test from");
    check(AddDirIfNot(fromroot) != 0, "AddDirIfNot failed when run from the volume root");
    check(is_dir(fromroot), "the folder was not created when run from the volume root");
    check(chdir_portable(here) == 0, "could not move back out of the volume root");

    if (g_failures == 0) {
        printf("OK: directory creation, from a fresh folder and from the volume root\n");
    }
    return (g_failures == 0) ? 0 : 1;
}
