/* Host regression test for the disc-image resource source (LIB386/SYSTEM/DISCIMG.CPP).
 * Builds a synthetic ISO9660 image whose game files are nested under a volume
 * directory (mirroring the real LBA2 disc, which puts everything under /LBA2),
 * writes it into a temp install dir, and pins that DiscImage:
 *   - mounts it by content-probing the dir (any extension),
 *   - detects the asset-root subtree by locating the resource marker (lba2.hqr),
 *   - resolves an absolute install path to the image by stripping the mount base,
 *     including the marker file itself (the image carries all game data),
 *   - streams a multi-sector file via the virtual handle (Read/Seek, incl.
 *     cross-sector and EOF), and sizes it with Seek-to-end (no slurp),
 *   - reports file and directory existence (the preflight music/ and vox/ checks),
 *   - reports nothing for paths outside the base or absent from the image.
 * SDL-free: links DISCIMG + ISO9660 + FILES + LOG (LBA_LOG_NO_SDL), no device. */
#include "test_harness.h"

#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/FILES.H>

#include <sys/stat.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef TEST_TMP_DIR
#define TEST_TMP_DIR "."
#endif

/* ── ISO9660 fixture builder (nested directories) ──────────────────────── */

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

/* Append a directory record to sector `s` at running offset *o. */
static void add_rec(uint8_t *s, int *o, uint32_t lba, uint32_t len, bool isdir,
                    const uint8_t *name, int nlen) {
    int reclen = 33 + nlen;
    if (reclen & 1)
        reclen++;
    uint8_t *r = s + *o;
    memset(r, 0, (size_t)reclen);
    r[0] = (uint8_t)reclen;
    wr_both32(r + 2, lba);
    wr_both32(r + 10, len);
    r[25] = isdir ? 0x02 : 0x00;
    wr_both16(r + 28, 1);
    r[32] = (uint8_t)nlen;
    memcpy(r + 33, name, (size_t)nlen);
    *o += reclen;
}

static void add_named(uint8_t *s, int *o, uint32_t lba, uint32_t len, bool isdir,
                      const char *name) {
    add_rec(s, o, lba, len, isdir, (const uint8_t *)name, (int)strlen(name));
}

/* Sector layout. Files nest under a "GAME" volume directory. */
static const uint32_t L_PVD = 16, L_ROOT = 18, L_GAME = 19;
static const uint32_t L_VIDEO = 20, L_VOX = 21, L_MUSIC = 22;
static const uint32_t L_HQR = 24, L_VID = 25 /* 25..27 */, L_VOXF = 28, L_WAV = 29;
static const uint32_t LEN_HQR = 100, LEN_VID = 5000, LEN_VOXF = 50, LEN_WAV = 40;
static const uint32_t NSECTORS = 32;

static std::vector<uint8_t> build_iso() {
    std::vector<std::vector<uint8_t> > sec(NSECTORS, std::vector<uint8_t>(2048, 0));
    const uint8_t dot = 0x00, dotdot = 0x01;
    int o;

    uint8_t *pvd = sec[L_PVD].data();
    pvd[0] = 0x01;
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 0x01;
    o = 156; /* root directory record lives at offset 156 of the PVD */
    add_rec(pvd, &o, L_ROOT, 2048, true, &dot, 1);

    uint8_t *root = sec[L_ROOT].data();
    o = 0;
    add_rec(root, &o, L_ROOT, 2048, true, &dot, 1);
    add_rec(root, &o, L_ROOT, 2048, true, &dotdot, 1);
    add_named(root, &o, L_GAME, 2048, true, "GAME");

    uint8_t *game = sec[L_GAME].data();
    o = 0;
    add_rec(game, &o, L_GAME, 2048, true, &dot, 1);
    add_rec(game, &o, L_ROOT, 2048, true, &dotdot, 1);
    add_named(game, &o, L_HQR, LEN_HQR, false, "LBA2.HQR;1");
    add_named(game, &o, L_VIDEO, 2048, true, "VIDEO");
    add_named(game, &o, L_VOX, 2048, true, "VOX");
    add_named(game, &o, L_MUSIC, 2048, true, "MUSIC");

    uint8_t *video = sec[L_VIDEO].data();
    o = 0;
    add_rec(video, &o, L_VIDEO, 2048, true, &dot, 1);
    add_rec(video, &o, L_GAME, 2048, true, &dotdot, 1);
    add_named(video, &o, L_VID, LEN_VID, false, "VIDEO.HQR;1");

    uint8_t *vox = sec[L_VOX].data();
    o = 0;
    add_rec(vox, &o, L_VOX, 2048, true, &dot, 1);
    add_rec(vox, &o, L_GAME, 2048, true, &dotdot, 1);
    add_named(vox, &o, L_VOXF, LEN_VOXF, false, "EN_GAM.VOX;1");

    uint8_t *music = sec[L_MUSIC].data();
    o = 0;
    add_rec(music, &o, L_MUSIC, 2048, true, &dot, 1);
    add_rec(music, &o, L_GAME, 2048, true, &dotdot, 1);
    add_named(music, &o, L_WAV, LEN_WAV, false, "T1.WAV;1");

    /* File contents: VIDEO.HQR carries a multi-sector pattern (byte i == i&0xFF). */
    for (uint32_t i = 0; i < LEN_HQR; i++)
        sec[L_HQR][i] = (uint8_t)'H';
    for (uint32_t i = 0; i < LEN_VID; i++)
        sec[L_VID + i / 2048][i % 2048] = (uint8_t)(i & 0xFF);
    for (uint32_t i = 0; i < LEN_VOXF; i++)
        sec[L_VOXF][i] = (uint8_t)'V';
    for (uint32_t i = 0; i < LEN_WAV; i++)
        sec[L_WAV][i] = (uint8_t)'W';

    /* Pack cooked-2048 (the reader auto-detects either layout; covered elsewhere). */
    std::vector<uint8_t> out((size_t)NSECTORS * 2048, 0);
    for (uint32_t i = 0; i < NSECTORS; i++)
        memcpy(out.data() + (size_t)i * 2048, sec[i].data(), 2048);
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

/* ── tests ─────────────────────────────────────────────────────────────── */

static std::string g_base; /* install dir (mount base), trailing slash */

static void setup_mount(void) {
    std::string dir = std::string(TEST_TMP_DIR) + "/discmnt";
    mkdir(dir.c_str(), 0777); /* ignore EEXIST */
    write_file(dir + "/disc.bin", build_iso());
    g_base = dir + "/";
}

static void test_mount_and_banner(void) {
    ASSERT_TRUE(DiscImage_Mount(g_base.c_str(), "lba2.hqr"));
    ASSERT_TRUE(DiscImage_IsMounted());

    char path[512] = "";
    S32 files = 0;
    ASSERT_TRUE(DiscImage_GetBannerInfo(path, sizeof(path), &files));
    ASSERT_TRUE(strstr(path, "disc.bin") != NULL);
    ASSERT_EQ_INT(4, (int)files); /* LBA2.HQR, VIDEO.HQR, EN_GAM.VOX, T1.WAV */
}

static void test_stream_video(void) {
    std::string p = g_base + "video/VIDEO.HQR";
    S32 h = DiscImage_OpenRead(p.c_str());
    ASSERT_TRUE(h > 0);
    ASSERT_TRUE(DiscImage_IsHandle(h));

    /* Size via Seek-to-end (the FileSize path), then back to start. */
    ASSERT_EQ_INT((int)LEN_VID, DiscImage_Seek(h, 0, SEEK_FROM_END));
    ASSERT_EQ_INT(0, DiscImage_Seek(h, 0, SEEK_FROM_START));

    /* Streamed read of the first 100 bytes matches the pattern. */
    uint8_t buf[256];
    ASSERT_EQ_INT(100, DiscImage_Read(h, buf, 100));
    bool ok = true;
    for (int i = 0; i < 100; i++)
        if (buf[i] != (uint8_t)(i & 0xFF))
            ok = false;
    ASSERT_TRUE(ok);

    /* Seek across the 2048 boundary and read a straddling slice. */
    ASSERT_EQ_INT(2040, DiscImage_Seek(h, 2040, SEEK_FROM_START));
    uint8_t slice[20];
    ASSERT_EQ_INT(20, DiscImage_Read(h, slice, 20));
    ok = true;
    for (int i = 0; i < 20; i++)
        if (slice[i] != (uint8_t)((2040 + i) & 0xFF))
            ok = false;
    ASSERT_TRUE(ok);

    /* Read clamps at EOF. */
    ASSERT_EQ_INT((int)(LEN_VID - 10), DiscImage_Seek(h, (S32)LEN_VID - 10, SEEK_FROM_START));
    ASSERT_EQ_INT(10, DiscImage_Read(h, buf, 256));
    ASSERT_EQ_INT(0, DiscImage_Read(h, buf, 256));

    ASSERT_EQ_INT(0, DiscImage_Close(h));
}

static void test_generic_resolution(void) {
    /* The marker file itself resolves from the image (it carries all game data),
       not just the media. */
    std::string hqr = g_base + "lba2.hqr";
    S32 h = DiscImage_OpenRead(hqr.c_str());
    ASSERT_TRUE(h > 0);
    ASSERT_EQ_INT((int)LEN_HQR, DiscImage_Seek(h, 0, SEEK_FROM_END));
    DiscImage_Close(h);

    /* A voice bank under vox/. */
    std::string vox = g_base + "vox/EN_GAM.VOX";
    h = DiscImage_OpenRead(vox.c_str());
    ASSERT_TRUE(h > 0);
    DiscImage_Close(h);
}

static void test_existence(void) {
    /* Directories (the preflight subtree checks). */
    ASSERT_TRUE(DiscImage_Exists((g_base + "vox/").c_str()));
    ASSERT_TRUE(DiscImage_Exists((g_base + "music/").c_str()));
    ASSERT_TRUE(DiscImage_Exists((g_base + "video").c_str()));
    /* Files. */
    ASSERT_TRUE(DiscImage_Exists((g_base + "video/VIDEO.HQR").c_str()));
    /* Absent in image, and outside the mount base. */
    ASSERT_TRUE(!DiscImage_Exists((g_base + "nope.hqr").c_str()));
    ASSERT_TRUE(!DiscImage_Exists("/somewhere/else/file.hqr"));
}

static void test_misses(void) {
    ASSERT_EQ_INT(0, (int)DiscImage_OpenRead((g_base + "nope.hqr").c_str()));
    ASSERT_EQ_INT(0, (int)DiscImage_OpenRead("/outside/the/base.hqr"));
}

static void test_unmount(void) {
    DiscImage_Unmount();
    ASSERT_TRUE(!DiscImage_IsMounted());
    ASSERT_EQ_INT(0, (int)DiscImage_OpenRead((g_base + "video/VIDEO.HQR").c_str()));
}

int main(void) {
    setup_mount();
    RUN_TEST(test_mount_and_banner);
    RUN_TEST(test_stream_video);
    RUN_TEST(test_generic_resolution);
    RUN_TEST(test_existence);
    RUN_TEST(test_misses);
    RUN_TEST(test_unmount);
    TEST_SUMMARY();
    return test_failures != 0;
}
