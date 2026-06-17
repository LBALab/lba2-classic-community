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
static const uint32_t NSECTORS = 20;

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
static std::vector<std::vector<uint8_t> > build_sectors() {
    std::vector<std::vector<uint8_t> > secs(NSECTORS, std::vector<uint8_t>(2048, 0));
    const uint32_t fileLen = (uint32_t)strlen(FILE_CONTENT);

    /* Primary Volume Descriptor at LBA 16. */
    uint8_t *pvd = secs[16].data();
    pvd[0] = 0x01; /* descriptor type: primary */
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 0x01; /* version */
    const uint8_t selfName = 0x00;
    wr_dirrec(pvd + 156, ROOT_LBA, 2048, true, &selfName, 1); /* root dir record */

    /* Root directory extent at LBA 18: ".", "..", then the file. */
    uint8_t *root = secs[ROOT_LBA].data();
    int o = 0;
    const uint8_t dot = 0x00, dotdot = 0x01;
    o += wr_dirrec(root + o, ROOT_LBA, 2048, true, &dot, 1);
    o += wr_dirrec(root + o, ROOT_LBA, 2048, true, &dotdot, 1);
    const char *fname = "HELLO.TXT;1";
    wr_dirrec(root + o, FILE_LBA, fileLen, false, (const uint8_t *)fname, (int)strlen(fname));

    /* File content at LBA 19. */
    memcpy(secs[FILE_LBA].data(), FILE_CONTENT, fileLen);
    return secs;
}

/* Pack logical sectors into a raw-2352 (Mode-1) or cooked-2048 image. In the raw
   layout the 2048 user bytes sit at offset 16; sync/header/ECC stay zero (the
   reader never touches them). */
static std::vector<uint8_t> pack(const std::vector<std::vector<uint8_t> > &secs, bool raw) {
    const int ss = raw ? 2352 : 2048;
    const int off = raw ? 16 : 0;
    std::vector<uint8_t> out((size_t)secs.size() * ss, 0);
    for (size_t i = 0; i < secs.size(); i++)
        memcpy(out.data() + i * ss + off, secs[i].data(), 2048);
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

static void check_image(bool raw) {
    std::string path = std::string(TEST_TMP_DIR) + "/iso_fixture_" + (raw ? "raw" : "cooked") + ".bin";
    if (!write_file(path, pack(build_sectors(), raw))) {
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

    /* iso_walk enumerates exactly the one file, as a '/'-rooted path. */
    std::vector<std::string> paths;
    ASSERT_EQ_INT(0, iso_walk(iso, collect_path, &paths));
    ASSERT_EQ_INT(1, (int)paths.size());
    if (paths.size() == 1)
        ASSERT_TRUE(paths[0] == "/HELLO.TXT");

    iso_close(iso);
    remove(path.c_str());
}

static void test_cooked_2048(void) { check_image(false); }
static void test_raw_2352(void) { check_image(true); }

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

int main(void) {
    RUN_TEST(test_cooked_2048);
    RUN_TEST(test_raw_2352);
    RUN_TEST(test_non_iso_rejected);
    TEST_SUMMARY();
    return test_failures != 0;
}
