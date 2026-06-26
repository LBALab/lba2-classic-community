// Host-only test for batched config writes in LIB386/SYSTEM/DEFFILE.CPP.
//
// WriteConfigFile rewrites ~30 keys per save. Without batching, each
// DefFileBufferWriteString does its own fsync + atomic rename -- correct but
// slow. Begin/EndBatch redirect the per-key writes to a private ".saving"
// working temp (ordinary buffered writes) and commit the whole batch with a
// single fsync + atomic rename at the end.
//
// What must hold:
//   - all keys land, and unrelated pre-existing keys are preserved (merge);
//   - the destination keeps its OLD content until EndBatch commits (crash
//     safety: a crash mid-batch leaves the live cfg fully intact);
//   - the working temp is gone afterwards;
//   - Begin/End guard against misuse (no nesting, End requires an active batch).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/DEFFILE.H>
#include <SYSTEM/FILES.H>

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, \
                         __LINE__, #cond);                              \
            std::abort();                                               \
        }                                                               \
    } while (0)

namespace {

const char *kCfgPath = "test_deffile_batch.tmp.cfg";
const char *kTempPath = "test_deffile_batch.tmp.cfg.saving"; // ".saving" suffix

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

bool Exists(const char *path) {
    FILE *f = std::fopen(path, "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
    return false;
}

} // namespace

int main() {
    char path[512];
    std::snprintf(path, sizeof(path), "%s", kCfgPath);

    // ── Test 1: a batch merges every key and preserves unrelated ones ───────
    {
        WriteFile(path, "Keep: untouched\r\nLanguage: French\r\n");

        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        CHECK(DefFileBufferBeginBatch() == TRUE);
        CHECK(DefFileBufferWriteString("Language", "English") == TRUE);
        CHECK(DefFileBufferWriteValue("Shadow", 1) == TRUE);
        CHECK(DefFileBufferWriteValue("DetailLevel", 3) == TRUE);
        CHECK(DefFileBufferWriteString("NewKey", "hello") == TRUE);
        CHECK(DefFileBufferEndBatch() == TRUE);

        const std::string after = ReadFile(path);
        CHECK(after.find("Language: English") != std::string::npos);
        CHECK(after.find("French") == std::string::npos);
        CHECK(after.find("Shadow: 1") != std::string::npos);
        CHECK(after.find("DetailLevel: 3") != std::string::npos);
        CHECK(after.find("NewKey: hello") != std::string::npos);
        CHECK(after.find("Keep: untouched") != std::string::npos); // merge
        CHECK(!Exists(kTempPath));                                 // temp gone
    }

    // ── Test 2: the destination keeps OLD content until EndBatch ────────────
    // The crash-safety guarantee: while the batch is in flight the live cfg is
    // never touched -- all writes go to the working temp. A crash mid-batch
    // therefore leaves the old complete config in place.
    {
        const std::string oldContent = "Language: French\r\nShadow: 0\r\n";
        WriteFile(path, oldContent);

        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        CHECK(DefFileBufferBeginBatch() == TRUE);
        CHECK(DefFileBufferWriteString("Language", "English") == TRUE);
        CHECK(DefFileBufferWriteValue("Shadow", 1) == TRUE);

        // Mid-batch: the real file still reads as the complete old config.
        CHECK(ReadFile(path) == oldContent);
        CHECK(Exists(kTempPath)); // work-in-progress lives in the temp

        CHECK(DefFileBufferEndBatch() == TRUE);

        // Now the new content is committed and the temp is gone.
        const std::string after = ReadFile(path);
        CHECK(after.find("Language: English") != std::string::npos);
        CHECK(after.find("Shadow: 1") != std::string::npos);
        CHECK(!Exists(kTempPath));
    }

    // ── Test 3: values written in a batch read back correctly after commit ──
    {
        WriteFile(path, "Seed: 1\r\n");
        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        CHECK(DefFileBufferBeginBatch() == TRUE);
        CHECK(DefFileBufferWriteValue("VoiceVolume", 112) == TRUE);
        CHECK(DefFileBufferWriteValue("MusicVolume", 96) == TRUE);
        CHECK(DefFileBufferEndBatch() == TRUE);

        // Re-init from the committed file and read the values back.
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);
        CHECK(DefFileBufferReadValue("VoiceVolume") == 112);
        CHECK(DefFileBufferReadValue("MusicVolume") == 96);
    }

    // ── Test 4: Begin/End guard against misuse ──────────────────────────────
    {
        WriteFile(path, "A: 1\r\n");
        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        // End with no active batch -> FALSE.
        CHECK(DefFileBufferEndBatch() == FALSE);

        // Begin twice without End -> second call refused (no nesting).
        CHECK(DefFileBufferBeginBatch() == TRUE);
        CHECK(DefFileBufferBeginBatch() == FALSE);
        CHECK(DefFileBufferEndBatch() == TRUE);

        // After a clean End, the next Begin works again.
        CHECK(DefFileBufferBeginBatch() == TRUE);
        CHECK(DefFileBufferEndBatch() == TRUE);
    }

    // ── Test 5: an empty batch leaves the destination untouched ─────────────
    {
        const std::string oldContent = "Only: thing\r\n";
        WriteFile(path, oldContent);
        std::vector<char> buffer(8192, 0);
        CHECK(DefFileBufferInit(path, &buffer[0], (S32)buffer.size()) == TRUE);

        CHECK(DefFileBufferBeginBatch() == TRUE);
        CHECK(DefFileBufferEndBatch() == TRUE); // no writes in between

        CHECK(ReadFile(path) == oldContent); // unchanged
        CHECK(!Exists(kTempPath));           // no temp created
    }

    std::remove(path);
    std::remove(kTempPath);
    std::printf("test_deffile_batch: all checks passed\n");
    return 0;
}
