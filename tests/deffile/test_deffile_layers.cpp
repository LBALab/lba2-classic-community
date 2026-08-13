// Host-only test for layered config reads in LIB386/SYSTEM/DEFFILE.CPP.
//
// The engine reads its config as the file a run owns with the one shipped
// beside the game data underneath it. Reads scan forward and stop at the first
// match, so the layer below only answers for keys the owned file leaves out.
//
// What must hold:
//   - the owned file shadows the layer, key for key;
//   - a key only the layer defines is still found;
//   - a key neither defines falls through to the caller's default;
//   - an absent layer is ordinary, not an error, and changes nothing;
//   - a layer carrying NULs is refused rather than attached, since one would
//     end every read that reached it and hide the layers below;
//   - writes refuse while a layer is attached, because rewriting the buffer
//     would persist the game data's config into the player's;
//   - save/restore round-trips the whole window, which is what lets a
//     transient read against another buffer happen mid-boot without dropping
//     the layer for everything that reads after it.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

const char *kOwnPath = "test_deffile_layers.own.cfg";
const char *kDataPath = "test_deffile_layers.data.cfg";

char g_buffer[64 * 1024];
char g_other[8 * 1024];

void WriteFile(const char *path, const std::string &contents) {
    FILE *f = std::fopen(path, "wb");
    CHECK(f != NULL);
    if (!contents.empty()) {
        std::fwrite(contents.data(), 1, contents.size(), f);
    }
    std::fclose(f);
}

std::string ReadKey(const char *ident) {
    const char *v = DefFileBufferReadStringDefault(ident, "<none>");
    return std::string(v ? v : "<null>");
}

void LoadLayered(bool withData) {
    CHECK(DefFileBufferInit((char *)kOwnPath, g_buffer, (S32)sizeof g_buffer));
    if (withData) {
        CHECK(DefFileBufferAppendFile(kDataPath));
    }
}

} // namespace

int main() {
    // Owned config: sets Language and Shadow, says nothing about Version.
    WriteFile(kOwnPath, "Language: English\nShadow: 3\n");
    // Game data underneath: a different Language, plus Version and a key only
    // it carries.
    WriteFile(kDataPath, "Language: Francais\nVersion: 3\nPathInstall: C:\\LBA2\n");

    // --- precedence ---------------------------------------------------------
    LoadLayered(true);
    CHECK(ReadKey("Language") == "English"); // owned shadows the layer
    CHECK(ReadKey("Shadow") == "3");         // owned only
    CHECK(ReadKey("Version") == "3");        // layer supplies what owned omits
    CHECK(ReadKey("PathInstall") == "C:\\LBA2");
    CHECK(ReadKey("NothingDefinesThis") == "<none>"); // falls to the default
    CHECK(DefFileBufferReadValueDefault("Version", 0) == 3);

    // Prefix matching must not confuse a longer key for a shorter one: reading
    // "Version" cannot answer from a "Version_US" line.
    WriteFile(kDataPath, "Version_US: 1\n");
    LoadLayered(true);
    CHECK(ReadKey("Version") == "<none>");
    CHECK(ReadKey("Version_US") == "1");

    // --- an absent or empty layer is ordinary -------------------------------
    std::remove(kDataPath);
    CHECK(DefFileBufferInit((char *)kOwnPath, g_buffer, (S32)sizeof g_buffer));
    CHECK(!DefFileBufferAppendFile(kDataPath)); // absent: false, not fatal
    CHECK(ReadKey("Language") == "English");    // owned still reads
    WriteFile(kDataPath, "");
    CHECK(!DefFileBufferAppendFile(kDataPath)); // empty: same
    CHECK(ReadKey("Language") == "English");

    // --- a corrupt layer is refused, not attached ---------------------------
    WriteFile(kDataPath, std::string("Version: 3\n\0\0junk\n", 18));
    CHECK(DefFileBufferInit((char *)kOwnPath, g_buffer, (S32)sizeof g_buffer));
    CHECK(!DefFileBufferAppendFile(kDataPath));
    CHECK(ReadKey("Version") == "<none>");   // the layer was not attached
    CHECK(ReadKey("Language") == "English"); // and the owned file survived

    // --- writes refuse while a layer is attached ----------------------------
    WriteFile(kDataPath, "Version: 3\n");
    LoadLayered(true);
    CHECK(!DefFileBufferWriteString("Shadow", "1"));
    CHECK(!DefFileBufferBeginBatch());
    // The owned file on disk is untouched by the refusal.
    CHECK(DefFileBufferInit((char *)kOwnPath, g_buffer, (S32)sizeof g_buffer));
    CHECK(ReadKey("Shadow") == "3");
    // A plain re-load clears the layer, so the ordinary writer works again.
    CHECK(DefFileBufferWriteString("Shadow", "1"));
    CHECK(ReadKey("Shadow") == "1");
    WriteFile(kOwnPath, "Language: English\nShadow: 3\n");

    // --- save/restore round-trips the window --------------------------------
    // This is the boot case: the layered read is assembled, then something does
    // a transient read against a buffer of its own, and everything that reads
    // afterwards must still see the layer.
    LoadLayered(true);
    T_DEFFILE_STATE saved;
    DefFileBufferSaveState(&saved);

    WriteFile(kDataPath, "Version: 9\n"); // a different file, read transiently
    CHECK(DefFileBufferInit((char *)kDataPath, g_other, (S32)sizeof g_other));
    CHECK(ReadKey("Version") == "9");
    CHECK(ReadKey("Language") == "<none>"); // the transient read sees only itself

    DefFileBufferRestoreState(&saved);
    CHECK(ReadKey("Language") == "English"); // owned layer back
    CHECK(ReadKey("Version") == "3");        // and the game-data layer with it
    // The restored state still knows it is layered, so writes still refuse.
    CHECK(!DefFileBufferWriteString("Shadow", "2"));

    std::remove(kOwnPath);
    std::remove(kDataPath);
    std::printf("test_deffile_layers: OK\n");
    return 0;
}
