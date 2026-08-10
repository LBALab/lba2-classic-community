//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/DISCIMG.H>

#include <SYSTEM/FILES.H> // ESeekOrigin values, IsDirectory
#include <SYSTEM/ISO9660.H>
#include <SYSTEM/LIMITS.H> // ADELINE_MAX_PATH
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
// Smallest file the container scan will bother looking inside. A game CD image
// is hundreds of megabytes; this only has to be well under that and well over
// the config files and executables sharing the directory.
#define DISC_MIN_IMAGE_BYTES (1024 * 1024)

//──────────────────────────────────────────────────────────────────────────
// Is `a` a better image to mount than the incumbent `b`? A raw 2352 image wins
// over a cooked 2048 one because it is a superset: the same filesystem plus the
// CD audio tracks a cooked image drops. Then the larger file, then the lower
// name, so the choice never depends on directory order (a dump directory often
// holds both a .bin and a .iso of the same disc, and picking by readdir order
// would silently lose the audio on some machines and not others).
static bool BetterCandidate(int aSectorSize, long aSize, const char *aName,
                            int bSectorSize, long bSize, const char *bName) {
    if (aSectorSize != bSectorSize)
        return aSectorSize > bSectorSize;
    if (aSize != bSize)
        return aSize > bSize;
    return strcmp(aName, bName) < 0;
}

//──────────────────────────────────────────────────────────────────────────
// An explicitly named image, from --disc. Empty when the directory scan decides.
static char g_overridePath[ADELINE_MAX_PATH] = "";

extern "C" void DiscImage_SetOverride(const char *path) {
    if (path == NULL) {
        g_overridePath[0] = '\0';
        return;
    }
    strncpy(g_overridePath, path, sizeof(g_overridePath) - 1);
    g_overridePath[sizeof(g_overridePath) - 1] = '\0';
}

//──────────────────────────────────────────────────────────────────────────
// Content-probe every file in `assetsDir` and return the best image, or NULL.
// Two passes: the plain probe over all candidates first, and only if that finds
// nothing, the container scan (which reads each file's head, so it is not worth
// paying for on an install that already has an ordinary image).
static iso9660_t *PickImage(const char *assetsDir, char *outPath, size_t outMax) {
    // Named explicitly: no ranking, and no falling back to the scan. Someone who
    // says which disc to use is owed either that disc or an error, not a
    // different one chosen quietly. The container scan is allowed here without a
    // second pass, since naming the file is the opt-in.
    if (g_overridePath[0] != '\0') {
        const char *img = g_overridePath;
        iso9660_t *iso = iso_open_ex(img, 1);
        if (iso == NULL) {
            Log_Error("DISCIMG: --disc %s is not a disc image we can read", img);
            return NULL;
        }
        strncpy(outPath, img, outMax - 1);
        outPath[outMax - 1] = '\0';
        return iso;
    }

    iso9660_t *best = NULL;
    int bestSectorSize = 0;
    long bestSize = 0;
    char bestName[ADELINE_MAX_PATH] = "";

    for (int pass = 0; pass < 2 && best == NULL; pass++) {
        DIR *dir = opendir(assetsDir);
        if (dir == NULL)
            return NULL;

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.')
                continue; // skip ., .., dotfiles
            char cand[ADELINE_MAX_PATH];
            int n = snprintf(cand, sizeof(cand), "%s%s", assetsDir, ent->d_name);
            if (n < 0 || (size_t)n >= sizeof(cand))
                continue;
            if (IsDirectory(cand))
                continue;

            long size = 0;
            FILE *f = fopen(cand, "rb");
            if (f != NULL) {
                if (fseek(f, 0, SEEK_END) == 0)
                    size = ftell(f);
                fclose(f);
            }
            // The container scan reads each candidate's head, so on the second
            // pass skip anything too small to be a disc image. Keeps the cost
            // off ordinary directories full of config files and executables.
            if (pass == 1 && size < DISC_MIN_IMAGE_BYTES)
                continue;

            iso9660_t *iso = iso_open_ex(cand, pass == 1);
            if (iso == NULL)
                continue;

            int sectorSize = 0;
            iso_layout(iso, &sectorSize, NULL);

            if (best == NULL ||
                BetterCandidate(sectorSize, size, ent->d_name, bestSectorSize, bestSize, bestName)) {
                if (best != NULL) {
                    Log_Debug("DISCIMG: %s passed over for %s", bestName, ent->d_name);
                    iso_close(best);
                }
                best = iso;
                bestSectorSize = sectorSize;
                bestSize = size;
                strncpy(bestName, ent->d_name, sizeof(bestName) - 1);
                bestName[sizeof(bestName) - 1] = '\0';
                strncpy(outPath, cand, outMax - 1);
                outPath[outMax - 1] = '\0';
            } else {
                Log_Debug("DISCIMG: %s passed over for %s", ent->d_name, bestName);
                iso_close(iso);
            }
        }
        closedir(dir);
    }
    return best;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" bool DiscImage_DirHoldsMarker(const char *dir, const char *resMarker) {
    if (dir == NULL || dir[0] == '\0' || resMarker == NULL || resMarker[0] == '\0')
        return false;

    char imagePath[ADELINE_MAX_PATH] = "";
    iso9660_t *iso = PickImage(dir, imagePath, sizeof(imagePath));
    if (iso == NULL)
        return false;

    WalkCtx ctx;
    ctx.marker = resMarker;
    ctx.rootOut[0] = '\0';
    ctx.found = 0;
    ctx.count = 0;
    iso_walk(iso, DiscWalkCb, &ctx);
    iso_close(iso);

    if (ctx.found)
        Log_Debug("DISCIMG: %s holds %s, dir accepted without extracted data", imagePath, resMarker);
    return ctx.found != 0;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" bool DiscImage_Mount(const char *assetsDir, const char *resMarker) {
    if (g_iso != NULL)
        return true; // already mounted
    if (assetsDir == NULL || assetsDir[0] == '\0')
        return false;

    char cand[ADELINE_MAX_PATH] = "";
    iso9660_t *iso = PickImage(assetsDir, cand, sizeof(cand));
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

    /* The boot banner's "Disc:" line (INITADEL.C) is the curated one-line report.
       Here we add the detail it omits (the full image path and the in-image asset
       root) at DEBUG, so it reaches adeline.log and the terminal for diagnosing
       asset-root/path issues without touching the banner. The console sink stays at
       INFO and is wired after this point, so the curated view is unaffected. */
    Log_Debug("DISCIMG: mounted %s (asset root '%s', %d files)", g_imagePath,
              g_assetRoot[0] ? g_assetRoot : "/", (int)g_fileCount);
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
extern "C" bool DiscImage_GetInfo(DiscImageInfo *out) {
    if (g_iso == NULL || out == NULL)
        return false;
    strncpy(out->path, g_imagePath, sizeof(out->path) - 1);
    out->path[sizeof(out->path) - 1] = '\0';
    strncpy(out->assetRoot, g_assetRoot, sizeof(out->assetRoot) - 1);
    out->assetRoot[sizeof(out->assetRoot) - 1] = '\0';
    out->sectorSize = 0;
    out->base = 0;
    iso_layout(g_iso, &out->sectorSize, &out->base);
    out->fileCount = g_fileCount;
    out->rawSectors = (U32)iso_raw_sector_count(g_iso);
    return true;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" U32 DiscImage_RawSectorCount(void) {
    return g_iso == NULL ? 0 : (U32)iso_raw_sector_count(g_iso);
}

//──────────────────────────────────────────────────────────────────────────
extern "C" U32 DiscImage_DataSectorCount(void) {
    return g_iso == NULL ? 0 : (U32)iso_volume_sectors(g_iso);
}

//──────────────────────────────────────────────────────────────────────────
extern "C" S32 DiscImage_ReadRawSectors(U32 lba, U32 count, void *buffer) {
    if (g_iso == NULL || buffer == NULL)
        return -1;
    return (S32)iso_read_raw(g_iso, lba, count, (uint8_t *)buffer);
}

//──────────────────────────────────────────────────────────────────────────
extern "C" bool DiscImage_IsMountedFile(const char *name) {
    if (g_iso == NULL || name == NULL || name[0] == '\0')
        return false;

    // Compare basenames: a cue names its image without a path, and the mount
    // knows the full filesystem path it opened.
    const char *mine = strrchr(g_imagePath, '/');
#ifdef _WIN32
    const char *bs = strrchr(g_imagePath, '\\');
    if (bs != NULL && (mine == NULL || bs > mine))
        mine = bs;
#endif
    mine = (mine != NULL) ? mine + 1 : g_imagePath;

    const char *theirs = strrchr(name, '/');
    const char *tbs = strrchr(name, '\\');
    if (tbs != NULL && (theirs == NULL || tbs > theirs))
        theirs = tbs;
    theirs = (theirs != NULL) ? theirs + 1 : name;

    return strcasecmp(mine, theirs) == 0;
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
        return true;                                    // a file
    return iso_list(g_iso, img, NoopListCb, NULL) == 0; // or a directory
}
