// Host-only test for the atomic (crash-safe) write primitives in
// LIB386/SYSTEM/FILES.CPP: OpenWriteAtomic / CommitWriteAtomic /
// AbortWriteAtomic / WriteFileAtomic.
//
// The config writer (DefFileBufferWriteString) and the embedded-default writer
// rely on these so a crash, kill, or power loss mid-write can never leave the
// live config truncated or half-written. The central invariant under test:
// while an atomic write is in flight, the destination file still holds its old
// complete content -- it only ever flips to the new content at commit time, in
// one atomic rename.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <dirent.h>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/FILES.H>

// CHECK survives Release/NDEBUG (unlike <cassert>'s assert): this test does real
// file I/O and must hard-abort on a failed precondition rather than press on.
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, \
                         __LINE__, #cond);                              \
            std::abort();                                               \
        }                                                               \
    } while (0)

namespace {

// CWD-relative paths: CTest runs each test in its build directory, which is
// always writable, and this behaves identically on POSIX, MSYS2, and Windows.
const char *kPath = "test_atomic_write.tmp.cfg";
const char *kPrefix = "test_atomic_write.tmp.cfg"; // temp files start with this

void WriteRaw(const char *path, const std::string &contents) {
    FILE *f = std::fopen(path, "wb");
    CHECK(f != NULL);
    if (!contents.empty())
        std::fwrite(contents.data(), 1, contents.size(), f);
    std::fclose(f);
}

bool FileExists(const char *path) {
    FILE *f = std::fopen(path, "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
    return false;
}

std::string ReadRaw(const char *path) {
    FILE *f = std::fopen(path, "rb");
    CHECK(f != NULL);
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    CHECK(n >= 0);
    std::fseek(f, 0, SEEK_SET);
    std::string out((size_t)n, '\0');
    if (n > 0)
        std::fread(&out[0], 1, (size_t)n, f);
    std::fclose(f);
    return out;
}

// Count temp scratch files the atomic writer may have left in the CWD. A clean
// commit/abort must leave none: names look like "<final>.tmp.<pid>.<n>".
int CountTempFiles() {
    int count = 0;
    DIR *d = opendir(".");
    CHECK(d != NULL);
    struct dirent *e;
    const std::string needle = std::string(kPrefix) + ".tmp.";
    while ((e = readdir(d)) != NULL) {
        if (std::strncmp(e->d_name, needle.c_str(), needle.size()) == 0)
            ++count;
    }
    closedir(d);
    return count;
}

void CleanUp() {
    std::remove(kPath);
    // Sweep any stray temp files so a failing run does not poison the next.
    DIR *d = opendir(".");
    if (!d)
        return;
    struct dirent *e;
    const std::string needle = std::string(kPrefix) + ".tmp.";
    while ((e = readdir(d)) != NULL) {
        if (std::strncmp(e->d_name, needle.c_str(), needle.size()) == 0)
            std::remove(e->d_name);
    }
    closedir(d);
}

} // namespace

int main() {
    CleanUp();

    // ── Test 1: WriteFileAtomic round-trips a whole buffer ──────────────────
    {
        const std::string data = "hello\r\nworld\r\n";
        CHECK(WriteFileAtomic(kPath, data.data(), (U32)data.size()) == 1);
        CHECK(ReadRaw(kPath) == data);
        CHECK(CountTempFiles() == 0); // no scratch left behind
    }

    // ── Test 2: WriteFileAtomic replaces existing content atomically ────────
    {
        WriteRaw(kPath, "OLD-CONTENT-THAT-MUST-VANISH");
        const std::string data = "NEW";
        CHECK(WriteFileAtomic(kPath, data.data(), (U32)data.size()) == 1);
        const std::string after = ReadRaw(kPath);
        CHECK(after == "NEW");
        CHECK(after.find("OLD") == std::string::npos);
        CHECK(CountTempFiles() == 0);
    }

    // ── Test 3: incremental Open/Write*/Commit concatenates fragments ───────
    // This is the shape DefFileBufferWriteString uses: one open, several Write
    // calls splicing around the changed key, one commit.
    {
        std::remove(kPath);
        S32 h = OpenWriteAtomic(kPath);
        CHECK(h != 0);
        CHECK(Write(h, "aaa", 3) == 3);
        CHECK(Write(h, "bbb", 3) == 3);
        CHECK(Write(h, "ccc", 3) == 3);
        CHECK(CommitWriteAtomic(h) == 1);
        CHECK(ReadRaw(kPath) == "aaabbbccc");
        CHECK(CountTempFiles() == 0);
    }

    // ── Test 4: the in-flight target keeps its OLD content until commit ─────
    // The whole point of the exercise. With plain OpenWrite the target would be
    // truncated to zero the instant it opened; here it must stay intact, byte
    // for byte, right up until the commit flips it in one step.
    {
        const std::string oldContent = "ORIGINAL-COMPLETE-CONFIG\r\n";
        WriteRaw(kPath, oldContent);

        S32 h = OpenWriteAtomic(kPath);
        CHECK(h != 0);
        CHECK(Write(h, "PARTIAL", 7) == 7);

        // Mid-write: a concurrent reader (or a crash recovery on next launch)
        // still sees the complete old file, never the partial new one.
        CHECK(ReadRaw(kPath) == oldContent);

        CHECK(Write(h, "-REST\r\n", 7) == 7);
        CHECK(CommitWriteAtomic(h) == 1);
        CHECK(ReadRaw(kPath) == "PARTIAL-REST\r\n");
        CHECK(CountTempFiles() == 0);
    }

    // ── Test 5: Abort discards the write, leaving the target untouched ──────
    {
        const std::string oldContent = "KEEP-ME\r\n";
        WriteRaw(kPath, oldContent);

        S32 h = OpenWriteAtomic(kPath);
        CHECK(h != 0);
        CHECK(Write(h, "throwaway", 9) == 9);
        AbortWriteAtomic(h);

        CHECK(ReadRaw(kPath) == oldContent); // unchanged
        CHECK(CountTempFiles() == 0);        // temp dropped
    }

    // ── Test 6: Abort on a never-existing target leaves none behind ─────────
    {
        std::remove(kPath);
        S32 h = OpenWriteAtomic(kPath);
        CHECK(h != 0);
        CHECK(Write(h, "data", 4) == 4);
        AbortWriteAtomic(h);
        CHECK(!FileExists(kPath)); // never created
        CHECK(CountTempFiles() == 0);
    }

    // ── Test 7: WriteFileAtomic handles a zero-byte buffer ──────────────────
    {
        CHECK(WriteFileAtomic(kPath, "", 0) == 1);
        CHECK(ReadRaw(kPath).empty());
        CHECK(CountTempFiles() == 0);
    }

    // ── Test 8: Commit/Abort on a bogus handle are safe no-ops ──────────────
    {
        CHECK(CommitWriteAtomic(0) == 0);
        CHECK(CommitWriteAtomic(424242) == 0); // unknown handle
        AbortWriteAtomic(0);                   // must not crash
        AbortWriteAtomic(424242);              // must not crash
    }

    // ── Test 9: more concurrent writers than slots; slots recycle ───────────
    // Four atomic writes can be in flight at once (ATOMIC_WRITE_SLOTS). A fifth
    // open while four are open must fail cleanly; after committing them, opening
    // succeeds again (no slot leak).
    {
        const char *paths[5] = {
            "test_atomic_write.tmp.cfg.a",
            "test_atomic_write.tmp.cfg.b",
            "test_atomic_write.tmp.cfg.c",
            "test_atomic_write.tmp.cfg.d",
            "test_atomic_write.tmp.cfg.e",
        };
        S32 h[5];
        for (int i = 0; i < 4; i++) {
            h[i] = OpenWriteAtomic(paths[i]);
            CHECK(h[i] != 0);
            CHECK(Write(h[i], "x", 1) == 1);
        }
        // Fifth concurrent open exceeds the slot table -> clean failure.
        h[4] = OpenWriteAtomic(paths[4]);
        CHECK(h[4] == 0);

        for (int i = 0; i < 4; i++)
            CHECK(CommitWriteAtomic(h[i]) == 1);

        // Slots freed: opening again now works.
        S32 again = OpenWriteAtomic(paths[4]);
        CHECK(again != 0);
        CHECK(Write(again, "y", 1) == 1);
        CHECK(CommitWriteAtomic(again) == 1);

        for (int i = 0; i < 5; i++) {
            CHECK(ReadRaw(paths[i]).size() == 1);
            std::remove(paths[i]);
        }
    }

    CleanUp();
    std::printf("test_atomic_write: all checks passed\n");
    return 0;
}
