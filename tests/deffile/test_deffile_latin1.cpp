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

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/DEFFILE.H>
#include <SYSTEM/FILES.H>

namespace {

std::string MakeTempCfgPath() {
    const char *tmp = std::getenv("TMPDIR");
    if (!tmp || !*tmp)
        tmp = "/tmp";
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s/lba2_deffile_test_%d.cfg", tmp, (int)getpid());
    return std::string(buf);
}

void WriteFile(const char *path, const std::string &contents) {
    FILE *f = std::fopen(path, "wb");
    assert(f != NULL);
    std::fwrite(contents.data(), 1, contents.size(), f);
    std::fclose(f);
}

std::string ReadFile(const char *path) {
    FILE *f = std::fopen(path, "rb");
    assert(f != NULL);
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string out(n, '\0');
    if (n > 0)
        std::fread(&out[0], 1, n, f);
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
    const std::string path = MakeTempCfgPath();
    char path_buf[512];
    std::snprintf(path_buf, sizeof(path_buf), "%s", path.c_str());

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
        WriteFile(path_buf, original);

        std::vector<char> buffer(8192, 0);
        S32 ok = DefFileBufferInit(path_buf, &buffer[0], (S32)buffer.size());
        assert(ok == TRUE);

        ok = DefFileBufferWriteString("Language", "English");
        assert(ok == TRUE);

        const std::string after = ReadFile(path_buf);
        // Old value gone, new value in place.
        assert(after.find("Français") == std::string::npos);
        assert(after.find("Language: English") != std::string::npos);
        // Bug A signature: stray "ais"-shaped suffix line after the rewrite.
        assert(after.find("\nais\r\n") == std::string::npos);
        assert(after.find("\nçais") == std::string::npos);
        // Subsequent lines preserved verbatim.
        assert(after.find("LanguageCD: English") != std::string::npos);
        assert(after.find("Input0_1: 42") != std::string::npos);
    }

    // ── Test 2: cumulative bleed across repeated rewrites (Bug A) ───────────
    // Bug A's hallmark is that the corruption *accumulates* — each rewrite
    // through a Latin-1-bearing field splices another partial copy in. Three
    // cycles back to "Français" must leave exactly one occurrence.
    {
        const std::string original =
            "Language: Français\r\n"
            "Input0_1: 1\r\n";
        WriteFile(path_buf, original);

        std::vector<char> buffer(8192, 0);
        assert(DefFileBufferInit(path_buf, &buffer[0], (S32)buffer.size()) == TRUE);

        assert(DefFileBufferWriteString("Language", "English") == TRUE);
        assert(DefFileBufferWriteString("Language", "Français") == TRUE);
        assert(DefFileBufferWriteString("Language", "English") == TRUE);
        assert(DefFileBufferWriteString("Language", "Français") == TRUE);

        const std::string after = ReadFile(path_buf);
        assert(CountOccurrences(after, "Français") == 1);
        assert(after.find("\nais\r\n") == std::string::npos);
        assert(after.find("Input0_1: 1") != std::string::npos);
    }

    // ── Test 3: DefFileBufferWriteString self-aliasing strcpy (Bug B) ───────
    // DefFileBufferWriteString re-enters DefFileBufferInit with the static
    // FileName buffer as the source argument; the guarded copy must not blow
    // up under ASan (strcpy-param-overlap). Behaviorally, the post-write
    // state must remain readable.
    {
        const std::string original = "Foo: bar\r\nBaz: qux\r\n";
        WriteFile(path_buf, original);

        std::vector<char> buffer(8192, 0);
        assert(DefFileBufferInit(path_buf, &buffer[0], (S32)buffer.size()) == TRUE);
        assert(DefFileBufferWriteString("Foo", "changed") == TRUE);

        char *foo = DefFileBufferReadString("Foo");
        assert(foo != NULL);
        assert(std::strcmp(foo, "changed") == 0);
        char *baz = DefFileBufferReadString("Baz");
        assert(baz != NULL);
        assert(std::strcmp(baz, "qux") == 0);
    }

    // ── Test 4: ReadBufferString does not desync past a Latin-1 line ────────
    // The inter-line skip loop at the bottom of ReadBufferString also used
    // signed comparisons. A high-bit byte in a line *preceding* the target
    // could cause the scanner to walk into the middle of the next line.
    {
        const std::string original =
            "Header: Français\r\n"
            "Target: hello\r\n";
        WriteFile(path_buf, original);

        std::vector<char> buffer(8192, 0);
        assert(DefFileBufferInit(path_buf, &buffer[0], (S32)buffer.size()) == TRUE);

        char *target = DefFileBufferReadString("Target");
        assert(target != NULL);
        assert(std::strcmp(target, "hello") == 0);
    }

    std::remove(path_buf);
    return 0;
}
