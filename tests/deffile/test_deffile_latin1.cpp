// Host-only regression test for LIB386/SYSTEM/DEFFILE.CPP (issue #115).
//
// Pins two bugs in the .cfg parser/writer:
//
//   A) Signed-char EOL detection treated high-bit bytes (Latin-1 / UTF-8
//      continuation bytes like 0xC3 in "Français") as control characters,
//      which truncated lines mid-token and made successive rewrites splice
//      garbage into the file. Repro: any cfg field containing a non-ASCII
//      byte, then any settings-change rewrite.
//
//   B) DefFileBufferWriteString re-initializes after a write by passing the
//      module-static FileName buffer back into DefFileBufferInit, where it
//      reaches strcpy(FileName, file) with source == dest — undefined
//      behavior, flagged by ASan as strcpy-param-overlap.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/DEFFILE.H>
#include <SYSTEM/FILES.H>

// CHECK() survives Release/NDEBUG (unlike <cassert>'s assert). The test uses real file
// I/O and must hard-abort if a precondition fails, otherwise downstream
// std::string(size, ...) constructions hit length_error on garbage sizes.
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, \
                         __LINE__, #cond);                              \
            std::abort();                                               \
        }                                                               \
    } while (0)

namespace {

// CWD-relative path: works identically on POSIX, MSYS2, and native Windows.
// CTest runs each test in its build directory, which is always writable.
const char *kTempCfgPath = "test_deffile_latin1.tmp.cfg";

void WriteFile(const char *path, const std::string &contents) {
    FILE *f = std::fopen(path, "wb");
    CHECK(f != NULL);
    std::fwrite(contents.data(), 1, contents.size(), f);
    std::fclose(f);
}

std::string ReadFile(const char *path) {
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

// True iff key `ident` is present and reads back exactly as `expected`.
bool ReadEq(const char *ident, const char *expected) {
    char *v = DefFileBufferReadString(ident);
    return v != NULL && std::strcmp(v, expected) == 0;
}

size_t CountOccurrences(const std::string &hay, const std::string &needle) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

int main() {
    // DefFileBufferInit takes a non-const char*; copy the path into a buffer.
    char path[512];
    std::snprintf(path, sizeof(path), "%s", kTempCfgPath);

    // ── Test 1: rewriting a field whose old value contains Latin-1 (Bug A) ──
    // The writer's "skip until EOL" loop at the start of the suffix copy uses
    // signed-char comparison. With "Français" as the old value, the 0xC3
    // continuation byte reads as negative, the skip stops mid-token, and the
    // trailing "ais\r\n" gets spliced into the file as a stray line after
    // the rewritten value.
    {
        const std::string original =
            "Language: Français\r\n"
            "LanguageCD: English\r\n"
            "Input0_1: 42\r\n";
        WriteFile(path, original);

        std::vector<char> buffer(8192, 0);
        S32 ok = DefFileBufferInit(path, &buffer[0], (S32)buffer.size());
        CHECK(ok == TRUE);

        ok = DefFileBufferWriteString("Language", "English");
        CHECK(ok == TRUE);

        const std::string after = ReadFile(path);
        // Old value gone, new value in place.
        CHECK(after.find("Français") == std::string::npos);
        CHECK(after.find("Language: English") != std::string::npos);
        // Bug A signature: stray "ais"-shaped suffix line after the rewrite.
        CHECK(after.find("\nais\r\n") == std::string::npos);
        CHECK(after.find("\nçais") == std::string::npos);
        // Subsequent lines preserved verbatim.
        CHECK(after.find("LanguageCD: English") != std::string::npos);
        CHECK(after.find("Input0_1: 42") != std::string::npos);
    }

    // ── Test 2: cumulative bleed across repeated rewrites (Bug A) ───────────
    // Bug A's hallmark is that the corruption *accumulates* — each rewrite
    // through a Latin-1-bearing field splices another partial copy in. Three
    // cycles back to "Français" must leave exactly one occurrence.
    {
        const std::string original =
            "Language: Français\r\n"
            "Input0_1: 1\r\n";
        WriteFile(path, original);

        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        CHECK(DefFileBufferWriteString("Language", "English") == TRUE);
        CHECK(DefFileBufferWriteString("Language", "Français") == TRUE);
        CHECK(DefFileBufferWriteString("Language", "English") == TRUE);
        CHECK(DefFileBufferWriteString("Language", "Français") == TRUE);

        const std::string after = ReadFile(path);
        CHECK(CountOccurrences(after, "Français") == 1);
        CHECK(after.find("\nais\r\n") == std::string::npos);
        CHECK(after.find("Input0_1: 1") != std::string::npos);
    }

    // ── Test 3: DefFileBufferWriteString self-aliasing strcpy (Bug B) ───────
    // DefFileBufferWriteString re-enters DefFileBufferInit with the static
    // FileName buffer as the source argument; the guarded copy must not blow
    // up under ASan (strcpy-param-overlap). Behaviorally, the post-write
    // state must remain readable.
    {
        const std::string original = "Foo: bar\r\nBaz: qux\r\n";
        WriteFile(path, original);

        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(DefFileBufferWriteString("Foo", "changed") == TRUE);

        char *foo = DefFileBufferReadString("Foo");
        CHECK(foo != NULL);
        CHECK(std::strcmp(foo, "changed") == 0);
        char *baz = DefFileBufferReadString("Baz");
        CHECK(baz != NULL);
        CHECK(std::strcmp(baz, "qux") == 0);
    }

    // ── Test 4: ReadBufferString does not desync past a Latin-1 line ────────
    // The inter-line skip loop at the bottom of ReadBufferString also used
    // signed comparisons. A high-bit byte in a line *preceding* the target
    // could cause the scanner to walk into the middle of the next line.
    {
        const std::string original =
            "Header: Français\r\n"
            "Target: hello\r\n";
        WriteFile(path, original);

        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        char *target = DefFileBufferReadString("Target");
        CHECK(target != NULL);
        CHECK(std::strcmp(target, "hello") == 0);
    }

    // ── Test 5: a NUL-corrupted cfg is discarded and self-heals on write ────
    // A partial/garbage write injects NUL bytes mid-file (the real-world repro:
    // a process killed mid-rewrite left ~1.5 KB of NULs after the first key).
    // The in-place load-modify-rewrite would otherwise propagate those NULs on
    // every save — sticky corruption that never recovers. DefFileBufferInit
    // must treat such a blob as empty so the next write regenerates a clean,
    // NUL-free file from defaults.
    {
        std::string corrupt = "FlagDisplayText: ON\r\n";
        corrupt.append(2000, '\0'); // garbage NUL block
        corrupt += "VoiceVolume: 112\r\n";
        WriteFile(path, corrupt);

        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        // Corrupt blob discarded -> every key reads as absent (caller defaults).
        CHECK(DefFileBufferReadString("FlagDisplayText") == NULL);
        CHECK(DefFileBufferReadString("VoiceVolume") == NULL);

        // Next write produces a clean, NUL-free file carrying the new value.
        CHECK(DefFileBufferWriteString("VoiceVolume", "100") == TRUE);
        const std::string healed = ReadFile(path);
        CHECK(healed.find('\0') == std::string::npos);
        CHECK(healed.find("VoiceVolume: 100") != std::string::npos);
    }

    // ── Test 6: leading garbage line is stripped, every real key survives ────
    // Real-world repro (Windows): a long run of one byte (0x4C 'L' -- a stale
    // palette index from the shared ScreenAux buffer) got stamped as line 1 of
    // lba2.cfg by an interrupted write. It has no ':' so the parser skips it and
    // the writer copies it back on every save -- sticky corruption the NUL guard
    // misses because the bytes are all printable. It also inflated the file past
    // the boot reader's scratch buffer, silently dropping ResolutionX/Y.
    // DefFileBufferInit must EXCISE only that line and keep every valid key, so
    // the settings read correctly immediately (before any write) and the file
    // heals to a clean, key-preserving state on the next write.
    {
        std::string corrupt(3000, 'L'); // stale-graphics run, no separator
        corrupt += "\r\n";
        corrupt += "FixedTimestep: 16\r\n";
        corrupt += "Input0_1: 82\r\n";
        corrupt += "VoiceVolume: 112\r\n";
        corrupt += "Language: Français\r\n";
        corrupt += "ResolutionX: 640\r\n";
        corrupt += "ResolutionY: 480\r\n";
        WriteFile(path, corrupt);

        std::vector<char> buffer(65536, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        // Immediately readable -- no write needed. Every key preserved, verbatim.
        CHECK(ReadEq("FixedTimestep", "16"));
        CHECK(ReadEq("Input0_1", "82"));
        CHECK(ReadEq("VoiceVolume", "112"));
        CHECK(ReadEq("Language", "Français")); // non-ASCII value intact past the strip
        CHECK(ReadEq("ResolutionX", "640"));   // the setting the boot read used to drop
        CHECK(ReadEq("ResolutionY", "480"));

        // A settings change persists a clean file: no 'L' run, all keys intact.
        CHECK(DefFileBufferWriteString("ResolutionX", "1280") == TRUE);
        const std::string healed = ReadFile(path);
        CHECK(healed.find("LLLL") == std::string::npos);
        CHECK(healed.find("ResolutionX: 1280") != std::string::npos);
        CHECK(healed.find("FixedTimestep: 16") != std::string::npos);
        CHECK(healed.find("VoiceVolume: 112") != std::string::npos);
        CHECK(healed.find("Language: Français") != std::string::npos);
        // Re-init the healed file: still clean, still complete.
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(ReadEq("ResolutionX", "1280"));
        CHECK(ReadEq("Input0_1", "82"));
    }

    // ── Test 7: garbage line in the MIDDLE -- keys on both sides survive ──────
    // Also pins that the parser does not desync across the excised line (the
    // Bug-A failure mode from Test 4, but for the strip path).
    {
        std::string corrupt = "FixedTimestep: 16\r\n";
        corrupt += "Shadow: 3\r\n";
        corrupt.append(2500, 'L'); // junk between two good blocks
        corrupt += "\r\n";
        corrupt += "MasterVolume: 127\r\n";
        corrupt += "DetailLevel: 3\r\n";
        WriteFile(path, corrupt);

        std::vector<char> buffer(65536, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(ReadEq("FixedTimestep", "16"));
        CHECK(ReadEq("Shadow", "3"));
        CHECK(ReadEq("MasterVolume", "127"));
        CHECK(ReadEq("DetailLevel", "3"));
    }

    // ── Test 8: trailing garbage with no final newline -- prior keys survive ─
    {
        std::string corrupt = "MenuMouse: 1\r\nVSync: 0\r\n";
        corrupt.append(1500, 'L'); // trailing junk, unterminated
        WriteFile(path, corrupt);

        std::vector<char> buffer(65536, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(ReadEq("MenuMouse", "1"));
        CHECK(ReadEq("VSync", "0"));

        // Heals on write: junk gone, keys kept.
        CHECK(DefFileBufferWriteString("VSync", "1") == TRUE);
        const std::string healed = ReadFile(path);
        CHECK(healed.find("LLLL") == std::string::npos);
        CHECK(healed.find("MenuMouse: 1") != std::string::npos);
        CHECK(healed.find("VSync: 1") != std::string::npos);
    }

    // ── Test 9: several garbage lines are all stripped, all keys survive ────
    {
        std::string corrupt(700, 'L');
        corrupt += "\r\n";
        corrupt += "AllCameras: 1\r\n";
        corrupt.append(900, 'X'); // a second junk run, different byte
        corrupt += "\r\n";
        corrupt += "ReverseStereo: 0\r\n";
        corrupt.append(600, 'Z');
        corrupt += "\r\n";
        WriteFile(path, corrupt);

        std::vector<char> buffer(65536, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(ReadEq("AllCameras", "1"));
        CHECK(ReadEq("ReverseStereo", "0"));
    }

    // ── Test 10: false-positive guard -- a long value WITH a ':' is kept ──────
    // Long lines are only junk when they lack a separator. A legitimate
    // "ident: <long value>" has its ':' up front, so even one past the 512-char
    // strip threshold must be kept. (Read-back can't verify the full value --
    // DefFileBufferReadString caps at DefString's 256 bytes -- so this checks the
    // line survives on disk and the following key still parses, i.e. no strip,
    // no desync.)
    {
        const std::string longval(600, 'x'); // line length 610 > 512, but has ':'
        std::string original = "LastSave: " + longval + "\r\nShadow: 3\r\n";
        WriteFile(path, original);

        std::vector<char> buffer(65536, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(ReadEq("Shadow", "3")); // key after the long line still parses

        // Rewriting a *different* key copies the buffer verbatim; the long
        // colon-bearing line must still be there -- proof it was never stripped.
        CHECK(DefFileBufferWriteString("Shadow", "3") == TRUE);
        CHECK(ReadFile(path).find(longval) != std::string::npos);
    }

    // ── Test 11: threshold boundary -- 512 no-':' chars kept, 513 stripped ────
    // Guards against both a too-eager strip (eating a merely-longish odd line)
    // and a too-lax one. 512 is the largest run treated as tolerable.
    {
        std::string keep = std::string(512, 'q') + "\r\nShadow: 2\r\n";
        WriteFile(path, keep);
        std::vector<char> buffer(65536, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(ReadEq("Shadow", "2"));
        CHECK(DefFileBufferWriteString("Shadow", "2") == TRUE);
        CHECK(ReadFile(path).find(std::string(512, 'q')) != std::string::npos); // kept

        std::string strip = std::string(513, 'q') + "\r\nShadow: 5\r\n";
        WriteFile(path, strip);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(ReadEq("Shadow", "5"));
        CHECK(DefFileBufferWriteString("Shadow", "5") == TRUE);
        CHECK(ReadFile(path).find(std::string(513, 'q')) == std::string::npos); // stripped
    }

    std::remove(path);
    return 0;
}
