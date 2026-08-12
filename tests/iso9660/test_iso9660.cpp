/* Host regression test for the vendored ISO9660 reader (LIB386/SYSTEM/iso9660.c).
 * Builds a minimal synthetic ISO9660 image entirely in-test (PVD at LBA 16, a root
 * directory holding one file), packs it both ways, and proves the reader:
 *   - auto-detects 2352-byte raw (Mode-1) and 2048-byte cooked sector layouts,
 *   - reads a known file by path (case-insensitively, ignoring the ";1" version),
 *   - reports a miss for an absent file,
 *   - enumerates the tree via iso_walk.
 * The reader is SDL-free and asset-free, so the test compiles it directly with no
 * engine, no disc image on disk beyond the fixture it writes to the build dir. */
#include "test_harness.h"

#include <SYSTEM/ISO9660.H>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef TEST_TMP_DIR
#define TEST_TMP_DIR "."
#endif

static const char *const FILE_CONTENT = "Hello, disc!\n";
static const uint32_t ROOT_LBA = 18;
static const uint32_t FILE_LBA = 19;
/* A second, multi-sector file to exercise streamed/cross-sector reads. Its
   bytes follow a known pattern: byte i == (i & 0xFF). 5000 bytes spans 3
   sectors (LBA 20..22). */
static const uint32_t BIG_LBA = 20;
static const uint32_t BIG_LEN = 5000;
static const uint32_t NSECTORS = 24;

/* ISO9660 stores 32-bit fields "both-endian": LE in the first 4 bytes, BE in the
   next 4. The reader only consumes the LE half, but we write a faithful field. */
static void wr_both32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
    p[4] = (uint8_t)((v >> 24) & 0xFF);
    p[5] = (uint8_t)((v >> 16) & 0xFF);
    p[6] = (uint8_t)((v >> 8) & 0xFF);
    p[7] = (uint8_t)(v & 0xFF);
}

static void wr_both16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

/* Write a directory record at dst and return its (even-padded) byte length. */
static int wr_dirrec(uint8_t *dst, uint32_t lba, uint32_t len, bool isdir,
                     const uint8_t *name, int nlen) {
    int reclen = 33 + nlen;
    if (reclen & 1)
        reclen++; /* records are padded to an even length */
    memset(dst, 0, (size_t)reclen);
    dst[0] = (uint8_t)reclen;
    dst[1] = 0; /* extended attribute length */
    wr_both32(dst + 2, lba);
    wr_both32(dst + 10, len);
    /* bytes 18..24: recording date/time, left zero (the reader ignores it) */
    dst[25] = isdir ? 0x02 : 0x00; /* file flags */
    dst[26] = 0;                   /* file unit size */
    dst[27] = 0;                   /* interleave gap */
    wr_both16(dst + 28, 1);        /* volume sequence number */
    dst[32] = (uint8_t)nlen;
    memcpy(dst + 33, name, (size_t)nlen);
    return reclen;
}

/* Build the logical 2048-byte sectors of a one-file image. */
static std::vector<std::vector<uint8_t>> build_sectors() {
    std::vector<std::vector<uint8_t>> secs(NSECTORS, std::vector<uint8_t>(2048, 0));
    const uint32_t fileLen = (uint32_t)strlen(FILE_CONTENT);

    /* Primary Volume Descriptor at LBA 16. */
    uint8_t *pvd = secs[16].data();
    pvd[0] = 0x01; /* descriptor type: primary */
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 0x01;                 /* version */
    wr_both32(pvd + 80, NSECTORS); /* volume space size, in logical blocks */
    wr_both16(pvd + 128, 2048);    /* logical block size */
    const uint8_t selfName = 0x00;
    wr_dirrec(pvd + 156, ROOT_LBA, 2048, true, &selfName, 1); /* root dir record */

    /* Root directory extent at LBA 18: ".", "..", then the file. */
    uint8_t *root = secs[ROOT_LBA].data();
    int o = 0;
    const uint8_t dot = 0x00, dotdot = 0x01;
    o += wr_dirrec(root + o, ROOT_LBA, 2048, true, &dot, 1);
    o += wr_dirrec(root + o, ROOT_LBA, 2048, true, &dotdot, 1);
    const char *fname = "HELLO.TXT;1";
    o += wr_dirrec(root + o, FILE_LBA, fileLen, false, (const uint8_t *)fname, (int)strlen(fname));
    const char *bname = "BIG.BIN;1";
    wr_dirrec(root + o, BIG_LBA, BIG_LEN, false, (const uint8_t *)bname, (int)strlen(bname));

    /* File content: the short file, then the patterned multi-sector file. */
    memcpy(secs[FILE_LBA].data(), FILE_CONTENT, fileLen);
    for (uint32_t i = 0; i < BIG_LEN; i++)
        secs[BIG_LBA + i / 2048][i % 2048] = (uint8_t)(i & 0xFF);
    return secs;
}

/* Pack logical sectors into one of the on-disc layouts. `sectorSize` is 2352 for
   a raw image or 2048 for a cooked one, and `dataOff` is where the 2048 user
   bytes start inside a sector: 16 for MODE1 raw, 24 for MODE2 Form 1 (the same
   sync and header plus an 8-byte subheader), 0 for cooked. Sync/header/subheader
   and ECC stay zero, which the reader never looks at. */
static std::vector<uint8_t> pack(const std::vector<std::vector<uint8_t>> &secs, int sectorSize,
                                 int dataOff) {
    std::vector<uint8_t> out((size_t)secs.size() * sectorSize, 0);
    for (size_t i = 0; i < secs.size(); i++)
        memcpy(out.data() + i * sectorSize + dataOff, secs[i].data(), 2048);
    return out;
}

static bool write_file(const std::string &path, const std::vector<uint8_t> &data) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f)
        return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    return ok;
}

/* iso_walk callback: collect each reported path. extern "C" so its linkage matches
   the C reader's function-pointer typedef. */
extern "C" void collect_path(void *ud, const char *path, uint32_t size) {
    (void)size;
    ((std::vector<std::string> *)ud)->push_back(path);
}

static void check_image(int sectorSize, int dataOff, const char *tag) {
    const bool raw = (sectorSize == 2352);
    std::string path = std::string(TEST_TMP_DIR) + "/iso_fixture_" + tag + ".bin";
    if (!write_file(path, pack(build_sectors(), sectorSize, dataOff))) {
        FAIL_MSG("could not write fixture %s", path.c_str());
        return;
    }

    iso9660_t *iso = iso_open(path.c_str());
    ASSERT_TRUE(iso != NULL);
    if (!iso) {
        remove(path.c_str());
        return;
    }

    const uint32_t fileLen = (uint32_t)strlen(FILE_CONTENT);

    /* Read the file by its exact name. */
    uint8_t *out = NULL;
    size_t sz = 0;
    ASSERT_EQ_INT(0, iso_read(iso, "HELLO.TXT", &out, &sz));
    ASSERT_EQ_INT((int)fileLen, (int)sz);
    ASSERT_TRUE(out != NULL && memcmp(out, FILE_CONTENT, fileLen) == 0);
    free(out);

    /* Lookup is case-insensitive and ignores the ";1" version suffix. */
    out = NULL;
    sz = 0;
    ASSERT_EQ_INT(0, iso_read(iso, "hello.txt", &out, &sz));
    free(out);

    /* An absent file is a clean miss, not a crash. */
    out = NULL;
    sz = 0;
    ASSERT_EQ_INT(-1, iso_read(iso, "NOPE.TXT", &out, &sz));

    /* iso_walk enumerates both files, each as a '/'-rooted path. */
    std::vector<std::string> paths;
    ASSERT_EQ_INT(0, iso_walk(iso, collect_path, &paths));
    ASSERT_EQ_INT(2, (int)paths.size());
    bool sawHello = false, sawBig = false;
    for (size_t i = 0; i < paths.size(); i++) {
        if (paths[i] == "/HELLO.TXT")
            sawHello = true;
        if (paths[i] == "/BIG.BIN")
            sawBig = true;
    }
    ASSERT_TRUE(sawHello && sawBig);

    /* iso_stat locates an extent without reading; iso_pread streams it. */
    uint32_t blba = 0, bsize = 0;
    ASSERT_EQ_INT(0, iso_stat(iso, "BIG.BIN", &blba, &bsize));
    ASSERT_EQ_INT((int)BIG_LEN, (int)bsize);
    ASSERT_EQ_INT(-1, iso_stat(iso, "NOPE.BIN", &blba, &bsize));

    /* Full read reproduces the generated pattern. */
    uint8_t big[BIG_LEN];
    ASSERT_EQ_INT((int)BIG_LEN, iso_pread(iso, blba, bsize, 0, big, BIG_LEN));
    bool patOk = true;
    for (uint32_t i = 0; i < BIG_LEN; i++)
        if (big[i] != (uint8_t)(i & 0xFF))
            patOk = false;
    ASSERT_TRUE(patOk);

    /* A slice straddling the 2048-byte sector boundary reads correctly. */
    uint8_t slice[20];
    ASSERT_EQ_INT(20, iso_pread(iso, blba, bsize, 2040, slice, 20));
    bool sliceOk = true;
    for (uint32_t i = 0; i < 20; i++)
        if (slice[i] != (uint8_t)((2040 + i) & 0xFF))
            sliceOk = false;
    ASSERT_TRUE(sliceOk);

    /* Where the data filesystem ends. On a mixed-mode disc this bounds the data
       track, so it is the floor for any audio track's start sector, and the
       check that stops a bad cue playing the filesystem as sound. */
    ASSERT_EQ_INT((int)NSECTORS, (int)iso_volume_sectors(iso));
    /* Raw images can carry audio sectors after the data; cooked ones cannot. */
    ASSERT_EQ_INT(raw ? (int)NSECTORS : 0, (int)iso_raw_sector_count(iso));

    /* The detected layout is reported back, and the mode is what distinguishes
       MODE1 from MODE2: both are 2352-byte raw images and differ only here. A
       wrong data offset still finds the filesystem when base absorbs it, but
       then iso_read_raw is off by that much and the audio comes out as noise. */
    int gotSs = 0, gotOff = 0;
    long gotBase = -1;
    ASSERT_EQ_INT(0, iso_layout(iso, &gotSs, &gotBase, &gotOff));
    ASSERT_EQ_INT(sectorSize, gotSs);
    ASSERT_EQ_INT(dataOff, gotOff);
    ASSERT_EQ_INT(0, (int)gotBase);

    /* Reads clamp at EOF; reading at/after EOF yields 0. */
    uint8_t tail[64];
    ASSERT_EQ_INT(10, iso_pread(iso, blba, bsize, BIG_LEN - 10, tail, 64));
    ASSERT_EQ_INT(0, iso_pread(iso, blba, bsize, BIG_LEN, tail, 64));

    iso_close(iso);
    remove(path.c_str());
}

static void test_cooked_2048(void) { check_image(2048, 0, "cooked"); }
static void test_raw_2352(void) { check_image(2352, 16, "raw"); }
/* MODE2 Form 1, the CD-ROM XA / CD-Bridge layout the European EA pressing of
   LBA2 uses. Its user data sits 8 bytes further into each sector than MODE1. */
static void test_raw_2352_mode2(void) { check_image(2352, 24, "raw_mode2"); }

/* A non-ISO file (no CD001 PVD at LBA 16) is rejected, not misread. */
static void test_non_iso_rejected(void) {
    std::string path = std::string(TEST_TMP_DIR) + "/iso_fixture_bogus.bin";
    std::vector<uint8_t> junk(64 * 1024, 0xAB);
    if (!write_file(path, junk)) {
        FAIL_MSG("could not write fixture %s", path.c_str());
        return;
    }
    iso9660_t *iso = iso_open(path.c_str());
    ASSERT_TRUE(iso == NULL);
    if (iso)
        iso_close(iso);
    remove(path.c_str());
}

/* ── container-wrapped images ──────────────────────────────────────────── */
/* Some dumps put the image behind a small header, so logical sector 0 is not at
   byte 0 and the plain probes miss it (a DAEMON Tools MDX prefixes 64 bytes).
   iso_open_ex with the scan enabled must find the volume descriptor and derive
   the base offset, and the plain iso_open must keep rejecting it (the scan costs
   a read of the file's head, so callers opt in only after a cheap pass fails). */
static void check_container(bool raw, size_t headerLen) {
    std::string path = std::string(TEST_TMP_DIR) + "/iso_container_" +
                       (raw ? "raw" : "cooked") + ".bin";
    std::vector<uint8_t> data(headerLen, 0x00);
    std::vector<uint8_t> img = pack(build_sectors(), raw ? 2352 : 2048, raw ? 16 : 0);
    data.insert(data.end(), img.begin(), img.end());
    if (!write_file(path, data)) {
        FAIL_MSG("could not write fixture %s", path.c_str());
        return;
    }

    ASSERT_TRUE(iso_open(path.c_str()) == NULL); /* plain probe still says no */

    iso9660_t *iso = iso_open_ex(path.c_str(), 1);
    ASSERT_TRUE(iso != NULL);
    if (!iso) {
        remove(path.c_str());
        return;
    }
    int ss = 0;
    long base = 0;
    ASSERT_EQ_INT(0, iso_layout(iso, &ss, &base, NULL));
    ASSERT_EQ_INT(raw ? 2352 : 2048, ss);
    ASSERT_EQ_INT((int)headerLen, (int)base);

    uint8_t *out = NULL;
    size_t sz = 0;
    ASSERT_EQ_INT(0, iso_read(iso, "HELLO.TXT", &out, &sz));
    ASSERT_TRUE(out != NULL && sz == strlen(FILE_CONTENT) &&
                memcmp(out, FILE_CONTENT, sz) == 0);
    free(out);
    iso_close(iso);
    remove(path.c_str());
}

static void test_container_header_cooked(void) { check_container(false, 64); }
static void test_container_header_raw(void) { check_container(true, 64); }

/* The scan must not accept a file that merely contains the descriptor signature.
   Here the bytes are present but the volume is not: no 2048-byte logical block
   size, no usable root extent. Both entry points must reject it. */
static void test_container_scan_rejects_lookalike(void) {
    std::string path = std::string(TEST_TMP_DIR) + "/iso_lookalike.bin";
    std::vector<uint8_t> junk(256 * 1024, 0xAB);
    const uint8_t sig[7] = {0x01, 'C', 'D', '0', '0', '1', 0x01};
    memcpy(junk.data() + 40000, sig, sizeof(sig)); /* plausible offset for a 2048 base */
    if (!write_file(path, junk)) {
        FAIL_MSG("could not write fixture %s", path.c_str());
        return;
    }
    iso9660_t *a = iso_open(path.c_str());
    ASSERT_TRUE(a == NULL);
    if (a)
        iso_close(a);
    iso9660_t *b = iso_open_ex(path.c_str(), 1);
    ASSERT_TRUE(b == NULL);
    if (b)
        iso_close(b);
    remove(path.c_str());
}

/* ── malformed / hostile image handling ────────────────────────────────── */
/* The image is essentially user-supplied input, so the reader must not read past
   its buffers or hang on crafted/corrupt on-disc structures. These build images
   that would trip an unguarded parser; run under the AddressSanitizer preset to
   catch any out-of-bounds regression. */

static std::vector<uint8_t> pack_cooked(const std::vector<std::vector<uint8_t>> &sec) {
    std::vector<uint8_t> out((size_t)sec.size() * 2048, 0);
    for (size_t i = 0; i < sec.size(); i++)
        memcpy(out.data() + i * 2048, sec[i].data(), 2048);
    return out;
}

/* Root directory ends with a record header at offset 2040 whose declared name
   would run past the 2048-byte extent. Eight rl=255 filler records push the
   cursor there first. The reader must report the 8 fillers and then stop. */
static std::vector<uint8_t> build_malformed_oob() {
    std::vector<std::vector<uint8_t>> sec(NSECTORS, std::vector<uint8_t>(2048, 0));
    uint8_t *pvd = sec[16].data();
    pvd[0] = 0x01;
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 0x01;
    const uint8_t selfName = 0x00;
    wr_dirrec(pvd + 156, ROOT_LBA, 2048, true, &selfName, 1);

    uint8_t *root = sec[ROOT_LBA].data();
    for (int i = 0; i < 8; i++) { // 8 * 255 = cursor reaches offset 2040
        uint8_t *r = root + i * 255;
        r[0] = 255;   // record length
        r[25] = 0x00; // file
        wr_both16(r + 28, 1);
        r[32] = 6; // name length
        memcpy(r + 33, "AAAAAA", 6);
    }
    root[2040] = 44; // nonzero rl: the record "starts" but runs off the extent end
    return pack_cooked(sec);
}

/* Valid PVD, but the root directory record points at an LBA past the image. */
static std::vector<uint8_t> build_truncated() {
    std::vector<std::vector<uint8_t>> sec(NSECTORS, std::vector<uint8_t>(2048, 0));
    uint8_t *pvd = sec[16].data();
    pvd[0] = 0x01;
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 0x01;
    const uint8_t selfName = 0x00;
    wr_dirrec(pvd + 156, NSECTORS + 50, 2048, true, &selfName, 1); // dangling extent
    return pack_cooked(sec);
}

static void test_malformed_record_bounded(void) {
    std::string path = std::string(TEST_TMP_DIR) + "/iso_malformed.bin";
    if (!write_file(path, build_malformed_oob())) {
        FAIL_MSG("could not write fixture %s", path.c_str());
        return;
    }
    iso9660_t *iso = iso_open(path.c_str());
    ASSERT_TRUE(iso != NULL);
    if (!iso) {
        remove(path.c_str());
        return;
    }
    std::vector<std::string> paths;
    ASSERT_EQ_INT(0, iso_walk(iso, collect_path, &paths)); // stops cleanly at the bad record
    ASSERT_EQ_INT(8, (int)paths.size());                   // only the well-formed fillers
    uint32_t lba = 0, size = 0;
    ASSERT_EQ_INT(-1, iso_stat(iso, "ZZZ.ZZZ", &lba, &size));
    iso_close(iso);
    remove(path.c_str());
}

static void test_truncated_extent_graceful(void) {
    std::string path = std::string(TEST_TMP_DIR) + "/iso_truncated.bin";
    if (!write_file(path, build_truncated())) {
        FAIL_MSG("could not write fixture %s", path.c_str());
        return;
    }
    iso9660_t *iso = iso_open(path.c_str()); // PVD is valid, so the image opens
    ASSERT_TRUE(iso != NULL);
    if (!iso) {
        remove(path.c_str());
        return;
    }
    std::vector<std::string> paths;
    ASSERT_EQ_INT(0, iso_walk(iso, collect_path, &paths)); // dangling root -> no entries
    ASSERT_EQ_INT(0, (int)paths.size());
    uint32_t lba = 0, size = 0;
    ASSERT_EQ_INT(-1, iso_stat(iso, "ANY.THING", &lba, &size));
    iso_close(iso);
    remove(path.c_str());
}

int main(void) {
    RUN_TEST(test_cooked_2048);
    RUN_TEST(test_raw_2352);
    RUN_TEST(test_raw_2352_mode2);
    RUN_TEST(test_non_iso_rejected);
    RUN_TEST(test_container_header_cooked);
    RUN_TEST(test_container_header_raw);
    RUN_TEST(test_container_scan_rejects_lookalike);
    RUN_TEST(test_malformed_record_bounded);
    RUN_TEST(test_truncated_extent_graceful);
    TEST_SUMMARY();
    return test_failures != 0;
}
