/**
 * Host-only tests for game data path resolution (no Docker / no ASM).
 */

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LIMITS.H>

#include "RES_DISCOVERY.H"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

extern "C" const U32 g_embeddedLba2CfgBytesSize;
extern "C" int WriteEmbeddedDefaultLba2Cfg(const char *destPath);

static void create_marker_hqr(const char *dir) {
    char marker[512];
    snprintf(marker, sizeof(marker), "%s/lba2.hqr", dir);
    FILE *f = fopen(marker, "wb");
    if (f) {
        fclose(f);
    }
}

static bool test_env_lba2_game_dir() {
    unsetenv("LBA2_GAME_DIR");
    char tmpl[] = "/tmp/lba2disc_test_XXXXXX";
    if (mkdtemp(tmpl) == NULL) {
        return false;
    }
    create_marker_hqr(tmpl);

    if (setenv("LBA2_GAME_DIR", tmpl, 1) != 0) {
        return false;
    }

    char out[ADELINE_MAX_PATH];
    int argc = 1;
    char arg0[] = "lba2";
    char *argv[] = {arg0, NULL};

    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    unsetenv("LBA2_GAME_DIR");

    if (!ok) {
        return false;
    }
    return strstr(out, tmpl) != NULL;
}

static bool test_argv_game_dir() {
    unsetenv("LBA2_GAME_DIR");
    char tmpl[] = "/tmp/lba2disc_arg_XXXXXX";
    if (mkdtemp(tmpl) == NULL) {
        return false;
    }
    create_marker_hqr(tmpl);

    char out[ADELINE_MAX_PATH];
    char arg0[] = "lba2";
    char arg1[] = "--game-dir";
    char arg2[512];
    strncpy(arg2, tmpl, sizeof(arg2));
    arg2[sizeof(arg2) - 1] = '\0';
    char *argv[] = {arg0, arg1, arg2, NULL};
    int argc = 3;

    const bool ok = ResolveGameDataDir(out, ADELINE_MAX_PATH, &argc, argv);
    if (!ok || argc != 1) {
        return false;
    }
    return strstr(out, tmpl) != NULL;
}

static bool test_embedded_cfg_write() {
    char dir[] = "/tmp/lba2emb_XXXXXX";
    if (mkdtemp(dir) == NULL) {
        return false;
    }
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/out.cfg", dir);

    if (!WriteEmbeddedDefaultLba2Cfg(dest)) {
        return false;
    }
    const U32 sz = FileSize(dest);
    unlink(dest);
    rmdir(dir);
    return sz == g_embeddedLba2CfgBytesSize;
}

int main() {
    int failed = 0;
    if (!test_env_lba2_game_dir()) {
        failed++;
    }
    if (!test_argv_game_dir()) {
        failed++;
    }
    if (!test_embedded_cfg_write()) {
        failed++;
    }
    return failed ? 1 : 0;
}
