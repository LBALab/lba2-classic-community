//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/ISO9660.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//──────────────────────────────────────────────────────────────────────────
struct iso9660 {
    FILE *f;
    int sector_size; // 2352 raw or 2048 cooked
    int data_off;    // byte offset of user data within a sector
    long base;       // byte offset of logical sector 0 (container header, usually 0)
    long file_size;  // total image size, for bounds-checking a probed layout
};

//──────────────────────────────────────────────────────────────────────────
// Little-endian 32-bit read. ISO9660 stores values "both-endian"; we consume
// the LE half.
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)(p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

//──────────────────────────────────────────────────────────────────────────
// Read the 2048 user bytes of logical sector `lba` into buf.
static int read_sector(iso9660_t *iso, uint32_t lba, uint8_t *buf2048) {
    long pos = iso->base + (long)lba * iso->sector_size + iso->data_off;
    if (fseek(iso->f, pos, SEEK_SET) != 0)
        return -1;
    return fread(buf2048, 1, 2048, iso->f) == 2048 ? 0 : -1;
}

//──────────────────────────────────────────────────────────────────────────
// Read `length` bytes starting at logical block `lba` (rounds up to sectors).
static uint8_t *read_extent(iso9660_t *iso, uint32_t lba, uint32_t length) {
    // Compute in 64-bit so a garbage `length` (read straight off the image) can't
    // wrap the +2047, and reject a sector count that would overflow the malloc
    // size on a 32-bit size_t. An over-large length then just fails (NULL), it
    // does not under-allocate and overrun.
    uint64_t nsect = ((uint64_t)length + 2047) / 2048;
    if (nsect == 0)
        nsect = 1;
    if (nsect > (uint64_t)(SIZE_MAX / 2048))
        return NULL;
    uint8_t *buf = (uint8_t *)malloc((size_t)nsect * 2048);
    if (!buf)
        return NULL;
    for (uint32_t s = 0; s < (uint32_t)nsect; s++) {
        if (read_sector(iso, lba + s, buf + (size_t)s * 2048) != 0) {
            free(buf);
            return NULL;
        }
    }
    return buf;
}

//──────────────────────────────────────────────────────────────────────────
// The two ordinary on-disc layouts: raw 2352 (16-byte sync+header before the
// user data) and cooked 2048. Both start the image at byte 0.
static const int LAYOUTS[][2] = {{2352, 16}, {2048, 0}};

//──────────────────────────────────────────────────────────────────────────
// How far into a file we are willing to look for a container header before the
// image proper. Real headers are tiny (a DAEMON Tools MDX puts 64 bytes there);
// the cap keeps the fallback scan bounded and stops a large unrelated file from
// being trawled end to end.
#define ISO_MAX_HEADER 65536

//──────────────────────────────────────────────────────────────────────────
// Try one candidate layout. `strict` adds the checks that make a scanned-for
// (rather than exactly-placed) descriptor trustworthy: the volume must declare
// 2048-byte logical blocks and its root extent must lie inside the file. The
// exact probes stay lenient so nothing that mounts today stops mounting.
static int probe_layout(iso9660_t *iso, int sectorSize, int dataOff, long base, int strict) {
    if (base < 0)
        return 0;
    iso->sector_size = sectorSize;
    iso->data_off = dataOff;
    iso->base = base;

    uint8_t s[2048];
    if (read_sector(iso, 16, s) != 0 || memcmp(s + 1, "CD001", 5) != 0)
        return 0;
    if (!strict)
        return 1;

    if (s[0] != 0x01 || s[6] != 0x01)
        return 0; // not a primary volume descriptor, version 1
    uint32_t logicalBlock = (uint32_t)(s[128] | ((uint32_t)s[129] << 8));
    if (logicalBlock != 2048)
        return 0;
    uint32_t rootLba = rd32(s + 156 + 2);
    long rootEnd = base + ((long)rootLba + 1) * sectorSize;
    return (rootLba != 0 && rootEnd > 0 && rootEnd <= iso->file_size) ? 1 : 0;
}

//──────────────────────────────────────────────────────────────────────────
// Fallback for an image wrapped in a container header (the image starts some
// bytes into the file, so sector 16 is not where the exact probes look). Scan
// the first ISO_MAX_HEADER + a descriptor's worth of bytes for a primary volume
// descriptor and work backwards to the base offset each layout would imply.
// One generic rule rather than per-format knowledge: it recognises the layout,
// not the container.
static int probe_container(iso9660_t *iso) {
    const size_t span = ISO_MAX_HEADER + 16 * 2352 + 2048;
    if (iso->file_size <= 0)
        return 0;
    size_t want = (size_t)iso->file_size < span ? (size_t)iso->file_size : span;
    uint8_t *buf = (uint8_t *)malloc(want);
    if (!buf)
        return 0;
    if (fseek(iso->f, 0, SEEK_SET) != 0 || fread(buf, 1, want, iso->f) != want) {
        free(buf);
        return 0;
    }

    int found = 0;
    static const uint8_t PVD_SIG[7] = {0x01, 'C', 'D', '0', '0', '1', 0x01};
    for (size_t p = 0; !found && p + sizeof(PVD_SIG) <= want; p++) {
        if (memcmp(buf + p, PVD_SIG, sizeof(PVD_SIG)) != 0)
            continue;
        for (size_t i = 0; i < sizeof(LAYOUTS) / sizeof(LAYOUTS[0]); i++) {
            long base = (long)p - LAYOUTS[i][1] - 16L * LAYOUTS[i][0];
            if (base < 0 || base > ISO_MAX_HEADER)
                continue;
            if (probe_layout(iso, LAYOUTS[i][0], LAYOUTS[i][1], base, 1)) {
                found = 1;
                break;
            }
        }
    }
    free(buf);
    return found;
}

//──────────────────────────────────────────────────────────────────────────
iso9660_t *iso_open_ex(const char *path, int allowContainerScan) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    iso9660_t *iso = (iso9660_t *)calloc(1, sizeof(*iso));
    if (!iso) {
        fclose(f);
        return NULL;
    }
    iso->f = f;
    if (fseek(f, 0, SEEK_END) == 0)
        iso->file_size = ftell(f);

    // Probe raw-2352 first (sync header is 16 bytes), then cooked-2048.
    for (size_t i = 0; i < sizeof(LAYOUTS) / sizeof(LAYOUTS[0]); i++) {
        if (probe_layout(iso, LAYOUTS[i][0], LAYOUTS[i][1], 0, 0))
            return iso;
    }
    if (allowContainerScan && probe_container(iso))
        return iso;

    fclose(f);
    free(iso);
    return NULL;
}

//──────────────────────────────────────────────────────────────────────────
iso9660_t *iso_open(const char *path) { return iso_open_ex(path, 0); }

//──────────────────────────────────────────────────────────────────────────
int iso_layout(iso9660_t *iso, int *outSectorSize, long *outBase) {
    if (iso == NULL)
        return -1;
    if (outSectorSize)
        *outSectorSize = iso->sector_size;
    if (outBase)
        *outBase = iso->base;
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
void iso_close(iso9660_t *iso) {
    if (!iso)
        return;
    if (iso->f)
        fclose(iso->f);
    free(iso);
}

//──────────────────────────────────────────────────────────────────────────
// Compare an ISO directory identifier (possibly "NAME.EXT;1") against a target
// component, case-insensitively, ignoring the version suffix.
static int name_eq(const uint8_t *id, int idlen, const char *target) {
    int n = idlen;
    for (int i = 0; i < idlen; i++) {
        if (id[i] == ';') {
            n = i;
            break;
        }
    }
    int tlen = (int)strlen(target);
    if (n != tlen)
        return 0;
    for (int i = 0; i < n; i++) {
        if (toupper(id[i]) != toupper((unsigned char)target[i]))
            return 0;
    }
    return 1;
}

//──────────────────────────────────────────────────────────────────────────
// Walk a directory extent for a child by name. want_dir selects file vs dir.
static int find_child(iso9660_t *iso, uint32_t dlba, uint32_t dlen,
                      const char *name, int want_dir, uint32_t *olba, uint32_t *olen) {
    uint8_t *buf = read_extent(iso, dlba, dlen);
    if (!buf)
        return -1;
    int rc = -1;
    uint32_t o = 0;
    while (o < dlen) {
        uint8_t rl = buf[o];
        if (rl == 0) { // records never span a sector; jump to the next one
            o = ((o / 2048) + 1) * 2048;
            continue;
        }
        if (o + 33 > dlen)
            break; // no room for a record header: stop (don't read past the extent)
        uint8_t nlen = buf[o + 32];
        if (rl < 33 + (uint32_t)nlen || o + rl > dlen)
            break; // malformed: record overruns the extent or can't hold its name
        uint32_t elba = rd32(buf + o + 2);
        uint32_t elen = rd32(buf + o + 10);
        uint8_t flags = buf[o + 25];
        const uint8_t *nm = buf + o + 33;
        int is_dir = (flags & 0x02) != 0;
        if (nlen > 1 && is_dir == want_dir && name_eq(nm, nlen, name)) {
            *olba = elba;
            *olen = elen;
            rc = 0;
            break;
        }
        o += rl;
    }
    free(buf);
    return rc;
}

//──────────────────────────────────────────────────────────────────────────
// Resolve a '/'-separated path to its (lba, length). The leaf is matched as a
// directory when leaf_is_dir is set, otherwise as a file.
static int resolve(iso9660_t *iso, const char *path, int leaf_is_dir,
                   uint32_t *olba, uint32_t *olen) {
    uint8_t s[2048];
    if (read_sector(iso, 16, s) != 0)
        return -1;
    // Root directory record lives at offset 156 of the primary descriptor.
    uint32_t lba = rd32(s + 156 + 2);
    uint32_t len = rd32(s + 156 + 10);

    const char *p = path;
    char comp[256];
    while (*p) {
        size_t k = 0;
        while (*p && *p != '/') {
            if (k < sizeof(comp) - 1)
                comp[k++] = *p;
            p++;
        }
        comp[k] = 0;
        while (*p == '/')
            p++;
        int last = (*p == 0);
        int want_dir = last ? leaf_is_dir : 1;
        uint32_t clba, clen;
        if (find_child(iso, lba, len, comp, want_dir, &clba, &clen) != 0)
            return -1;
        lba = clba;
        len = clen;
    }
    *olba = lba;
    *olen = len;
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
int iso_list(iso9660_t *iso, const char *dirpath, iso_list_cb cb, void *ud) {
    uint32_t lba, len;
    if (resolve(iso, dirpath, 1, &lba, &len) != 0)
        return -1;
    uint8_t *buf = read_extent(iso, lba, len);
    if (!buf)
        return -1;
    uint32_t o = 0;
    while (o < len) {
        uint8_t rl = buf[o];
        if (rl == 0) {
            o = ((o / 2048) + 1) * 2048;
            continue;
        }
        if (o + 33 > len)
            break;
        uint8_t nlen = buf[o + 32];
        if (rl < 33 + (uint32_t)nlen || o + rl > len)
            break;
        uint32_t elen = rd32(buf + o + 10);
        uint8_t flags = buf[o + 25];
        const uint8_t *nm = buf + o + 33;
        if (nlen > 1 && !(flags & 0x02)) {
            char name[256];
            int n = 0;
            for (int i = 0; i < nlen && nm[i] != ';' && n < 255; i++)
                name[n++] = (char)nm[i];
            name[n] = 0;
            cb(ud, name, elen);
        }
        o += rl;
    }
    free(buf);
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
#define ISO_PATH_MAX 1024

static void walk_dir(iso9660_t *iso, uint32_t dlba, uint32_t dlen, char *path,
                     size_t pathlen, int depth, iso_walk_cb cb, void *ud) {
    if (depth > 8)
        return;
    uint8_t *buf = read_extent(iso, dlba, dlen);
    if (!buf)
        return;
    uint32_t o = 0;
    while (o < dlen) {
        uint8_t rl = buf[o];
        if (rl == 0) {
            o = ((o / 2048) + 1) * 2048;
            continue;
        }
        if (o + 33 > dlen)
            break;
        uint8_t nlen = buf[o + 32];
        if (rl < 33 + (uint32_t)nlen || o + rl > dlen)
            break;
        uint32_t elba = rd32(buf + o + 2);
        uint32_t elen = rd32(buf + o + 10);
        uint8_t flags = buf[o + 25];
        const uint8_t *nm = buf + o + 33;
        if (nlen > 1) { // skip the '.' and '..' self/parent entries
            char name[256];
            int n = 0;
            for (int i = 0; i < nlen && nm[i] != ';' && n < 255; i++)
                name[n++] = (char)nm[i];
            name[n] = 0;
            size_t cur = pathlen;
            if (cur + 1 + (size_t)n < ISO_PATH_MAX) {
                path[cur] = '/';
                memcpy(path + cur + 1, name, (size_t)n + 1);
                if (flags & 0x02)
                    walk_dir(iso, elba, elen, path, cur + 1 + n, depth + 1, cb, ud);
                else
                    cb(ud, path, elen);
                path[cur] = 0;
            }
        }
        o += rl;
    }
    free(buf);
}

//──────────────────────────────────────────────────────────────────────────
int iso_walk(iso9660_t *iso, iso_walk_cb cb, void *ud) {
    uint8_t s[2048];
    if (read_sector(iso, 16, s) != 0)
        return -1;
    uint32_t lba = rd32(s + 156 + 2);
    uint32_t len = rd32(s + 156 + 10);
    char path[ISO_PATH_MAX];
    path[0] = 0;
    walk_dir(iso, lba, len, path, 0, 0, cb, ud);
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
int iso_read(iso9660_t *iso, const char *filepath, uint8_t **out, size_t *outsize) {
    uint32_t lba, len;
    if (resolve(iso, filepath, 0, &lba, &len) != 0)
        return -1;
    uint8_t *buf = read_extent(iso, lba, len);
    if (!buf)
        return -1;
    *out = buf; // over-allocated to a sector boundary; that's fine
    *outsize = len;
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
int iso_stat(iso9660_t *iso, const char *filepath, uint32_t *out_lba, uint32_t *out_size) {
    uint32_t lba, len;
    if (resolve(iso, filepath, 0, &lba, &len) != 0)
        return -1;
    if (out_lba)
        *out_lba = lba;
    if (out_size)
        *out_size = len;
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
int iso_pread(iso9660_t *iso, uint32_t lba, uint32_t size, uint32_t offset,
              uint8_t *out, uint32_t n) {
    if (offset >= size)
        return 0;
    if (n > size - offset)
        n = size - offset;

    uint32_t done = 0;
    uint8_t sec[2048];
    while (done < n) {
        uint32_t fileOff = offset + done;
        uint32_t s = lba + fileOff / 2048;
        uint32_t within = fileOff % 2048;
        if (read_sector(iso, s, sec) != 0)
            return done > 0 ? (int)done : -1;
        uint32_t take = 2048 - within;
        if (take > n - done)
            take = n - done;
        memcpy(out + done, sec + within, take);
        done += take;
    }
    return (int)done;
}
