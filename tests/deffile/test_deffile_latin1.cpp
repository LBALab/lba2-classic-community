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
#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,  \
                         __LINE__, #cond);                               \
            std::abort();                                                \
        }                                                                \
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

    std::remove(path);
    return 0;
}
