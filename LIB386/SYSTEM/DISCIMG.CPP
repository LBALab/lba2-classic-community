//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/DISCIMG.H>

#include <SYSTEM/FILES.H>   // ESeekOrigin values, IsDirectory
#include <SYSTEM/ISO9660.H>
#include <SYSTEM/LIMITS.H>  // ADELINE_MAX_PATH
#include <SYSTEM/LOG.H>

#include <dirent.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // strcasecmp

//──────────────────────────────────────────────────────────────────────────
// One mounted image per process. The ISO9660 reader keeps the image file open
// for the session; every read seeks within it, so interleaved handles are fine
// on the single asset-loading thread (concurrent audio streaming is a later
// concern).
static iso9660_t *g_iso = NULL;
static char g_imagePath[ADELINE_MAX_PATH] = ""; // filesystem path of the image
static char g_mountBase[ADELINE_MAX_PATH] = ""; // install dir (mount base, trailing sep)
static char g_assetRoot[ADELINE_MAX_PATH] = ""; // in-image asset root, no slashes (e.g. "LBA2")
static S32 g_fileCount = 0;

// Virtual handles are tagged well above any real POSIX fd so FILES.CPP can tell
// them apart with a single comparison.
#define DISC_HANDLE_BASE 0x40000000
#define DISC_MAX_HANDLES 16

struct DiscHandle {
    int used;
    uint32_t lba;  // file extent start, from iso_stat
    uint32_t size; // file length
    uint32_t pos;  // current read position
};
static DiscHandle g_handles[DISC_MAX_HANDLES];

//──────────────────────────────────────────────────────────────────────────
// Locate the asset root (parent of the resource marker) and count files.
namespace {
struct WalkCtx {
    const char *marker;
    char rootOut[ADELINE_MAX_PATH];
    int found;
    S32 count;
};
} // namespace

static void DiscWalkCb(void *ud, const char *path, uint32_t size) {
    (void)size;
    WalkCtx *c = (WalkCtx *)ud;
    c->count++;
    if (c->found || c->marker == NULL || c->marker[0] == '\0')
        return;

    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    if (strcasecmp(base, c->marker) != 0)
        return;

    // Asset root = parent directory of the marker, with surrounding slashes
    // stripped. "/LBA2/LBA2.HQR" -> "LBA2"; "/LBA2.HQR" -> "" (image root).
    size_t plen = (size_t)(base - path);
    char tmp[ADELINE_MAX_PATH];
    if (plen >= sizeof(tmp))
        plen = sizeof(tmp) - 1;
    memcpy(tmp, path, plen);
    tmp[plen] = '\0';

    const char *p = tmp;
    while (*p == '/')
        p++;
    size_t n = strlen(p);
    while (n > 0 && p[n - 1] == '/')
        n--;
    if (n >= sizeof(c->rootOut))
        n = sizeof(c->rootOut) - 1;
    memcpy(c->rootOut, p, n);
    c->rootOut[n] = '\0';
    c->found = 1;
}

//──────────────────────────────────────────────────────────────────────────
// Map an absolute filesystem path under the mount base to its in-image path
// (asset root + remainder, separators normalized to '/', no leading slash).
// Returns false if nothing is mounted or the path is not under the base.
static bool MapToImagePath(const char *fsPath, char *outImg, size_t outMax) {
    if (g_iso == NULL || fsPath == NULL)
        return false;
    size_t baseLen = strlen(g_mountBase);
    if (baseLen == 0 || strncmp(fsPath, g_mountBase, baseLen) != 0)
        return false;

    const char *rel = fsPath + baseLen;
    while (*rel == '/' || *rel == '\\')
        rel++;

    int n;
    if (g_assetRoot[0])
        n = snprintf(outImg, outMax, "%s/%s", g_assetRoot, rel);
    else
        n = snprintf(outImg, outMax, "%s", rel);
    if (n < 0 || (size_t)n >= outMax)
        return false;

    for (char *q = outImg; *q; q++)
        if (*q == '\\')
            *q = '/';
    return true;
}

//──────────────────────────────────────────────────────────────────────────
static DiscHandle *HandleSlot(S32 handle) {
    if (!DiscImage_IsHandle(handle))
        return NULL;
    DiscHandle *h = &g_handles[handle - DISC_HANDLE_BASE];
    return h->used ? h : NULL;
}

static void NoopListCb(void *ud, const char *name, uint32_t size) {
    (void)ud;
    (void)name;
    (void)size;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" bool DiscImage_Mount(const char *assetsDir, const char *resMarker) {
    if (g_iso != NULL)
        return true; // already mounted
    if (assetsDir == NULL || assetsDir[0] == '\0')
        return false;

    DIR *dir = opendir(assetsDir);
    if (dir == NULL)
        return false;

    char cand[ADELINE_MAX_PATH];
    iso9660_t *iso = NULL;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.')
            continue; // skip ., .., dotfiles
        int n = snprintf(cand, sizeof(cand), "%s%s", assetsDir, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(cand))
            continue;
        if (IsDirectory(cand))
            continue;
        iso = iso_open(cand);
        if (iso != NULL)
            break; // first ISO9660 image wins
    }
    closedir(dir);

    if (iso == NULL)
        return false;

    WalkCtx ctx;
    ctx.marker = resMarker;
    ctx.rootOut[0] = '\0';
    ctx.found = 0;
    ctx.count = 0;
    iso_walk(iso, DiscWalkCb, &ctx);

    g_iso = iso;
    strncpy(g_imagePath, cand, sizeof(g_imagePath));
    g_imagePath[sizeof(g_imagePath) - 1] = '\0';
    strncpy(g_mountBase, assetsDir, sizeof(g_mountBase));
    g_mountBase[sizeof(g_mountBase) - 1] = '\0';
    strncpy(g_assetRoot, ctx.rootOut, sizeof(g_assetRoot));
    g_assetRoot[sizeof(g_assetRoot) - 1] = '\0';
    g_fileCount = ctx.count;
    memset(g_handles, 0, sizeof(g_handles));

    /* The mount is reported by the boot banner's "Disc:" line (INITADEL.C), so
       no log here: a Log_Debug would print above the banner (it runs earlier)
       and duplicate that line. */
    return true;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" void DiscImage_Unmount(void) {
    if (g_iso != NULL) {
        iso_close(g_iso);
        g_iso = NULL;
    }
    g_imagePath[0] = '\0';
    g_mountBase[0] = '\0';
    g_assetRoot[0] = '\0';
    g_fileCount = 0;
    memset(g_handles, 0, sizeof(g_handles));
}

//──────────────────────────────────────────────────────────────────────────
extern "C" bool DiscImage_IsMounted(void) { return g_iso != NULL; }

//──────────────────────────────────────────────────────────────────────────
extern "C" bool DiscImage_GetBannerInfo(char *outPath, size_t outPathSize, S32 *outFileCount) {
    if (g_iso == NULL)
        return false;
    if (outPath != NULL && outPathSize > 0) {
        strncpy(outPath, g_imagePath, outPathSize);
        outPath[outPathSize - 1] = '\0';
    }
    if (outFileCount != NULL)
        *outFileCount = g_fileCount;
    return true;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" bool DiscImage_IsHandle(S32 handle) {
    return handle >= DISC_HANDLE_BASE && handle < DISC_HANDLE_BASE + DISC_MAX_HANDLES;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" S32 DiscImage_OpenRead(const char *filename) {
    if (g_iso == NULL)
        return 0;

    char img[ADELINE_MAX_PATH];
    if (!MapToImagePath(filename, img, sizeof(img)))
        return 0;

    uint32_t lba = 0, size = 0;
    if (iso_stat(g_iso, img, &lba, &size) != 0)
        return 0; // not a file in the image

    for (int i = 0; i < DISC_MAX_HANDLES; i++) {
        if (!g_handles[i].used) {
            g_handles[i].used = 1;
            g_handles[i].lba = lba;
            g_handles[i].size = size;
            g_handles[i].pos = 0;
            return DISC_HANDLE_BASE + i;
        }
    }
    Log_Warn("DISCIMG: out of virtual handles opening %s", filename);
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" S32 DiscImage_Read(S32 handle, void *buffer, U32 maxBytes) {
    DiscHandle *h = HandleSlot(handle);
    if (h == NULL || buffer == NULL)
        return -1;
    int got = iso_pread(g_iso, h->lba, h->size, h->pos, (uint8_t *)buffer, maxBytes);
    if (got > 0)
        h->pos += (uint32_t)got;
    return got;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" S32 DiscImage_Seek(S32 handle, S32 offset, S32 origin) {
    DiscHandle *h = HandleSlot(handle);
    if (h == NULL)
        return -1;

    long np;
    switch (origin) {
    case SEEK_FROM_START:
        np = offset;
        break;
    case SEEK_FROM_CURRENT:
        np = (long)h->pos + offset;
        break;
    case SEEK_FROM_END:
        np = (long)h->size + offset;
        break;
    default:
        return -1;
    }
    if (np < 0)
        np = 0;
    if (np > (long)h->size)
        np = (long)h->size;
    h->pos = (uint32_t)np;
    return (S32)h->pos;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" S32 DiscImage_Close(S32 handle) {
    DiscHandle *h = HandleSlot(handle);
    if (h == NULL)
        return -1;
    h->used = 0;
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" bool DiscImage_Exists(const char *path) {
    if (g_iso == NULL)
        return false;

    char img[ADELINE_MAX_PATH];
    if (!MapToImagePath(path, img, sizeof(img)))
        return false;

    uint32_t lba = 0, size = 0;
    if (iso_stat(g_iso, img, &lba, &size) == 0)
        return true; // a file
    return iso_list(g_iso, img, NoopListCb, NULL) == 0; // or a directory
}
