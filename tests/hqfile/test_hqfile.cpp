// Host test for the HQR entry reader in LIB386/SYSTEM/HQFILE.CPP.
//
// An HQR starts with a table of 4-byte offsets; the first offset is also the
// table's size, and the last one is the end of the file rather than an entry.
// Each entry is a 10-byte header (size, compressed size, method) and its data.
// HQF_Init must refuse anything the file does not hold, so HQF_LoadClose never
// unpacks with sizes left from an earlier load: that is what made the console's
// `cube 222` die with SIGBUS (#765).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/HQFILE.H>

// CHECK survives Release/NDEBUG, unlike <cassert>'s assert.
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, \
                         __LINE__, #cond);                              \
            std::abort();                                               \
        }                                                               \
    } while (0)

namespace {

typedef std::vector<U8> Bytes;

void PutU32(Bytes &b, size_t at, U32 v) {
    b[at] = (U8)v;
    b[at + 1] = (U8)(v >> 8);
    b[at + 2] = (U8)(v >> 16);
    b[at + 3] = (U8)(v >> 24);
}

struct Entry {
    bool empty;
    U32 size;
    U32 compressed;
    S16 method;
    std::string data;
};

Entry Stored(const std::string &s) {
    Entry e = {false, (U32)s.size(), (U32)s.size(), 0, s};
    return e;
}

Entry Empty() {
    Entry e = {true, 0, 0, 0, ""};
    return e;
}

// An HQR with these entries, then the end-of-file offset the real files carry.
Bytes Build(const std::vector<Entry> &entries) {
    const size_t table = (entries.size() + 1) * 4;
    Bytes b(table, 0);
    for (size_t i = 0; i < entries.size(); i++) {
        const Entry &e = entries[i];
        if (e.empty) {
            continue;
        }
        PutU32(b, i * 4, (U32)b.size());
        const size_t at = b.size();
        b.resize(at + 10);
        PutU32(b, at, e.size);
        PutU32(b, at + 4, e.compressed);
        b[at + 8] = (U8)e.method;
        b[at + 9] = (U8)(e.method >> 8);
        b.insert(b.end(), e.data.begin(), e.data.end());
    }
    PutU32(b, entries.size() * 4, (U32)b.size());
    return b;
}

const char *kPath = "test_hqfile.tmp.hqr";

void Write(const Bytes &b) {
    FILE *f = std::fopen(kPath, "wb");
    CHECK(f != NULL);
    if (!b.empty()) {
        CHECK(std::fwrite(&b[0], 1, b.size(), f) == b.size());
    }
    std::fclose(f);
}

// HQF_Init + HQF_LoadClose into a buffer with the RECOVER_AREA callers add.
std::string Load(S32 index, U32 *initSize) {
    std::vector<U8> buf(4096, 0xAA);
    *initSize = HQF_Init(kPath, index);
    const U32 got = HQF_LoadClose(&buf[0]);
    CHECK(HQF_File == 0); // LoadClose always closes
    return std::string((const char *)&buf[0], got);
}

void TestEntriesLoad() {
    Write(Build({Stored("hello world"), Empty(), Stored("second")}));
    U32 size;
    CHECK(Load(0, &size) == "hello world" && size == 11);
    CHECK(Load(2, &size) == "second" && size == 6);
    CHECK(HQF_ResSize(kPath, 2) == 6);
    CHECK(HQF_NbRes((char *)kPath) == 3);
}

void TestCompressedEntryLoads() {
    // LZSS (method 1) with one flag byte of eight literals: "ABCDEFGH".
    std::string lz = std::string(1, (char)0xFF) + "ABCDEFGH";
    Entry e = {false, 8, 9, 1, lz};
    Write(Build({e}));
    U32 size;
    CHECK(Load(0, &size) == "ABCDEFGH" && size == 8);
}

// The #765 crash: the end-of-file offset, read right after a good load.
void TestEndOfFileOffsetIsRefused() {
    Write(Build({Stored("hello world"), Stored("second")}));
    U32 size;
    CHECK(Load(0, &size) == "hello world");
    CHECK(Load(2, &size).empty() && size == 0);
    CHECK(HQF_ResSize(kPath, 2) == 0);
}

void TestOutOfTableIsRefused() {
    Write(Build({Stored("hello world")}));
    U32 size;
    CHECK(Load(0, &size) == "hello world");
    CHECK(Load(-1, &size).empty() && size == 0);
    CHECK(Load(3, &size).empty() && size == 0);
    CHECK(Load(0x40000000, &size).empty() && size == 0); // index * 4 overflows S32
}

void TestEmptySlotIsRefused() {
    Write(Build({Stored("hello world"), Empty()}));
    U32 size;
    CHECK(Load(0, &size) == "hello world");
    CHECK(Load(1, &size).empty() && size == 0);
}

void TestTruncatedFilesAreRefused() {
    U32 size;

    // Shorter than the table's first word.
    Write(Bytes(2, 0));
    CHECK(Load(0, &size).empty() && size == 0);

    // Header promises more data than the file holds.
    Bytes b = Build({Stored("hello world")});
    b.resize(b.size() - 4);
    Write(b);
    CHECK(Load(0, &size).empty());

    // An offset that points past the end of the file.
    b = Build({Stored("hello world")});
    PutU32(b, 4, 0x7FFFFFF0);
    Write(b);
    CHECK(Load(1, &size).empty() && size == 0);
}

// HQF_LoadClose puts a compressed entry so that it ends RECOVER_AREA past its
// size; a bigger compressed size would start before the caller's buffer.
void TestCompressedSizeTooBigIsRefused() {
    Entry e = {false, 8, 8 + 512 + 1, 1, std::string(8 + 512 + 1, 'x')};
    Write(Build({e}));
    U32 size;
    CHECK(Load(0, &size).empty() && size == 0);
}

} // namespace

int main() {
    TestEntriesLoad();
    TestCompressedEntryLoads();
    TestEndOfFileOffsetIsRefused();
    TestOutOfTableIsRefused();
    TestEmptySlotIsRefused();
    TestTruncatedFilesAreRefused();
    TestCompressedSizeTooBigIsRefused();
    std::remove(kPath);
    std::printf("test_hqfile: all passed\n");
    return 0;
}
