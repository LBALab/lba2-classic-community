/* Host test: keyboard bindings surviving a trip through the config file.
 *
 * ReadInputConfig and WriteInputConfig are the only things standing between a
 * player's remapped keys and the next launch, and both carry comments about
 * cases that cost a real bug to learn. Neither case was checked anywhere.
 *
 * What this pins:
 *
 *   - A round trip. Bindings written out come back identical, and the folded
 *     table comes back with them, because ReadInputConfig ends in InitInput.
 *
 *   - The first-exit case. A fresh profile reaches its first exit with no
 *     config file at all, and WriteInputConfig has to create one. It used to
 *     refuse, which dropped every binding made in the first session while the
 *     settings written after it created the file anyway.
 *
 *   - The all-zero-bindings guard, in all four of the shapes it has to tell
 *     apart. An empty keyboard table is not by itself proof of corruption:
 *     clearing every keyboard binding is something the remap screen can do, and
 *     a pad-only layout looks exactly like it. The pad bindings in the cfg are
 *     what separate the two, and they have to be read from the file rather than
 *     from GamepadKeys, because ReadGamepadConfig has not run yet at that point
 *     in the boot.
 *
 *   - WinMode. A cfg without it is a pre-Windows-95 config, and the layout it
 *     describes is not this one, so the whole file is skipped in favour of the
 *     defaults.
 *
 *   - The pad half of both. The guard reasons about what ReadGamepadConfig is
 *     going to install, so it is only sound while that prediction holds and
 *     while a pad binding written out is the one that comes back.
 */
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <string>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/DEFFILE.H>
#include <SYSTEM/INPUT.H>         // Input, GetInput, ClearNoRepeatInput
#include <SYSTEM/KEYBOARD_KEYS.H> // K_*

#include "INPUT.H" // SOURCES/INPUT.H

#include "held_keys.h"

static int fails = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            std::fprintf(stderr, "%s:%d: FAIL: ", __FILE__, \
                         __LINE__);                         \
            std::fprintf(stderr, __VA_ARGS__);              \
            std::fprintf(stderr, "\n");                     \
            fails++;                                        \
        }                                                   \
    } while (0)

namespace {

const char *kCfgPath = "test_input_config.cfg";

char g_buffer[64 * 1024];

void RemoveCfg(void) { std::remove(kCfgPath); }

void WriteCfg(const std::string &contents) {
    FILE *f = std::fopen(kCfgPath, "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", kCfgPath);
        std::abort();
    }
    std::fwrite(contents.data(), 1, contents.size(), f);
    std::fclose(f);
}

bool CfgExists(void) {
    FILE *f = std::fopen(kCfgPath, "rb");
    if (!f)
        return false;
    std::fclose(f);
    return true;
}

void OpenCfg(void) {
    if (!DefFileBufferInit((char *)kCfgPath, g_buffer, (S32)sizeof g_buffer)) {
        std::fprintf(stderr, "DefFileBufferInit failed on %s\n", kCfgPath);
        std::abort();
    }
}

std::string Line(const char *fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return std::string(buf);
}

// A layout that is nobody's default, so a value that survives the trip can only
// have come from the file. Every slot gets a distinct pair.
void SetProbeLayout(void) {
    for (int n = 0; n < MAX_INPUT; n++) {
        DefKeys[n].Key1 = (U32)(100 + n);
        DefKeys[n].Key2 = (U32)(200 + n);
    }
}

void ScribbleBindings(void) {
    for (int n = 0; n < MAX_INPUT; n++) {
        DefKeys[n].Key1 = 0xDEAD;
        DefKeys[n].Key2 = 0xBEEF;
    }
}

void ScribblePad(void) {
    for (int n = 0; n < MAX_INPUT; n++) {
        GamepadKeys[n].Key1 = 0xDEAD;
        GamepadKeys[n].Key2 = 0xBEEF;
    }
}

bool PadIsDefault(void) {
    for (int n = 0; n < MAX_INPUT; n++) {
        if (GamepadKeys[n].Key1 != GamepadKeysDefault[n].Key1 ||
            GamepadKeys[n].Key2 != GamepadKeysDefault[n].Key2)
            return false;
    }
    return true;
}

bool BindingsAreProbeLayout(void) {
    for (int n = 0; n < MAX_INPUT; n++) {
        if (DefKeys[n].Key1 != (U32)(100 + n) || DefKeys[n].Key2 != (U32)(200 + n))
            return false;
    }
    return true;
}

// Compares against the defaults without leaving either live table disturbed:
// RestoreInput writes both, and later checks in the same case still read them.
bool BindingsAreDefaults(void) {
    T_DEF_KEY savedKeys[MAX_INPUT];
    T_DEF_KEY savedPad[MAX_INPUT];
    std::memcpy(savedKeys, DefKeys, sizeof savedKeys);
    std::memcpy(savedPad, GamepadKeys, sizeof savedPad);

    RestoreInput();
    const bool same = std::memcmp(savedKeys, DefKeys, sizeof savedKeys) == 0;

    std::memcpy(DefKeys, savedKeys, sizeof savedKeys);
    std::memcpy(GamepadKeys, savedPad, sizeof savedPad);
    return same;
}

U32 InputFromHolding(U32 key) {
    ClearNoRepeatInput();
    HoldOnly(key);
    GetInput(0);
    return Input;
}

// Loads a hand-written cfg the way the boot does, with the live tables poisoned
// first so anything that comes back has to have come from the file.
void LoadCfg(const std::string &contents) {
    RemoveCfg();
    WriteCfg(contents);
    OpenCfg();
    ScribbleBindings();
    ReadInputConfig();
}

// --- 1. Round trip ----------------------------------------------------------
void CheckRoundTrip(void) {
    RemoveCfg();
    OpenCfg();

    SetProbeLayout();
    WriteInputConfig();

    ScribbleBindings();
    ReadInputConfig();

    CHECK(BindingsAreProbeLayout(),
          "bindings did not survive the write/read round trip");

    // ReadInputConfig ends in InitInput, so what came back is live rather than
    // just sitting in DefKeys. Slot 12 is I_PAUSE; its written Key1 was 112.
    CHECK(InputFromHolding(112) == I_PAUSE,
          "the round trip left the folded table stale");
}

// --- 2. First exit, with no file to write into ------------------------------
void CheckFirstExitCreatesFile(void) {
    RemoveCfg();
    CHECK(!CfgExists(), "precondition: no config file");

    OpenCfg(); // a fresh profile: init against a file that is not there
    SetProbeLayout();
    WriteInputConfig();

    CHECK(CfgExists(), "the first exit must create the config file");

    ScribbleBindings();
    OpenCfg();
    ReadInputConfig();
    CHECK(BindingsAreProbeLayout(),
          "bindings made in the first session must survive it");
}

// Every pad binding written out as zero, except one slot left on its default.
// Spelling the zeros out matters: an absent Gamepad key is not the same thing
// as one set to zero, since the guard defaults an absent read to the value
// ReadGamepadConfig would install.
std::string ClearedPad(int boundSlot) {
    std::string cfg;
    for (int n = 0; n < MAX_INPUT; n++) {
        if (n == boundSlot) {
            cfg += Line("Gamepad%d: %u\nGamepad%d_2: 0\n", n,
                        (U32)K_GAMEPAD_A, n);
        } else {
            cfg += Line("Gamepad%d: 0\nGamepad%d_2: 0\n", n, n);
        }
    }
    return cfg;
}

// --- 3. The all-zero guard --------------------------------------------------
// Four cfgs that all have an empty keyboard table and must not all be treated
// the same way.
void CheckAllZeroGuard(void) {
    std::string clearedKeyboard = "WinMode: 1\n";
    for (int n = 0; n < MAX_INPUT; n++)
        clearedKeyboard += Line("Input%d_1: 0\nInput%d_2: 0\n", n, n);

    // (a) Nothing bound anywhere. No key moves the hero, and the remap screen
    // needs the very bindings that are gone, so the defaults come back.
    LoadCfg(clearedKeyboard + ClearedPad(-1));
    CHECK(BindingsAreDefaults(),
          "a cfg with no usable bindings at all must restore the defaults");

    // (b) The same empty keyboard, with one pad button bound. That is a
    // pad-only layout, which is a choice: the empty keyboard stands. Slot 8 is
    // mid-table, so the guard has to walk to it rather than stop at the first
    // entry it reads.
    LoadCfg(clearedKeyboard + ClearedPad(8));
    CHECK(!BindingsAreDefaults(),
          "a pad-only layout must not be mistaken for a corrupt cfg");
    for (int n = 0; n < MAX_INPUT; n++) {
        CHECK(DefKeys[n].Key1 == 0 && DefKeys[n].Key2 == 0,
              "slot %d: the cleared keyboard binding must stay cleared", n);
    }

    // (c) A cfg predating gamepad support: empty keyboard, no Gamepad keys at
    // all. The guard defaults each pad read exactly as ReadGamepadConfig will,
    // so it asks what the pad bindings are going to BE, and the answer is the
    // default pad layout. Playable, so no restore.
    LoadCfg(clearedKeyboard);
    CHECK(!BindingsAreDefaults(),
          "a pre-gamepad cfg must not be mistaken for a corrupt cfg");
    for (int n = 0; n < MAX_INPUT; n++) {
        CHECK(DefKeys[n].Key1 == 0 && DefKeys[n].Key2 == 0,
              "slot %d: a pre-gamepad cfg's cleared keyboard must stay cleared",
              n);
    }

    // The other half of that same decision, and the reason the guard defaults
    // its probe the way it does rather than reading zero. Boot order runs
    // ReadGamepadConfig after ReadInputConfig, so the pad the guard predicted
    // has to be the pad that then arrives. If the two defaulting sites ever
    // disagree, the guard keeps a cfg that leaves nothing bound at all, which
    // is the exact state it exists to prevent.
    ScribblePad();
    ReadGamepadConfig();
    CHECK(PadIsDefault(),
          "the pad the guard predicted is not the pad ReadGamepadConfig installs");

    // (d) One keyboard binding left is enough on its own, pad or no pad.
    std::string oneKey = "WinMode: 1\n";
    for (int n = 0; n < MAX_INPUT; n++)
        oneKey += Line("Input%d_1: %u\nInput%d_2: 0\n", n,
                       n == 7 ? (U32)K_SPACE : 0u, n);
    LoadCfg(oneKey + ClearedPad(-1));
    CHECK(DefKeys[7].Key1 == (U32)K_SPACE && DefKeys[0].Key1 == 0,
          "one surviving keyboard binding is enough to keep the cfg");
}

// --- 4. No WinMode ----------------------------------------------------------
void CheckWinModeAbsentTakesDefaults(void) {
    std::string noWinMode;
    for (int n = 0; n < MAX_INPUT; n++)
        noWinMode += Line("Input%d_1: %d\nInput%d_2: 0\n", n, 100 + n, n);

    LoadCfg(noWinMode);
    CHECK(BindingsAreDefaults(),
          "a cfg with no WinMode describes another layout; the defaults win");

    // And WriteInputConfig stamps WinMode, so a file this port writes is never
    // read back that way.
    SetProbeLayout();
    WriteInputConfig();
    OpenCfg();
    ScribbleBindings();
    ReadInputConfig();
    CHECK(BindingsAreProbeLayout(), "a config this port wrote must read back");
}

// --- 5. The pad bindings make the same trip ---------------------------------
// Not for its own sake: the guard above is only sound while a pad binding
// written out is the pad binding that comes back.
void CheckGamepadRoundTrip(void) {
    RemoveCfg();
    OpenCfg();

    for (int n = 0; n < MAX_INPUT; n++) {
        GamepadKeys[n].Key1 = (U32)(300 + n);
        GamepadKeys[n].Key2 = (U32)(400 + n);
    }
    WriteGamepadConfig();

    ScribblePad();
    ReadGamepadConfig();

    for (int n = 0; n < MAX_INPUT; n++) {
        CHECK(GamepadKeys[n].Key1 == (U32)(300 + n) &&
                  GamepadKeys[n].Key2 == (U32)(400 + n),
              "slot %d: pad binding did not survive the round trip", n);
    }

    // The camera preferences ReadGamepadConfig also carries have clamps of
    // their own. Those belong to the increment that gives the pad its own
    // surface; what matters here is only that the bindings survive.
}

} // namespace

int main() {
    CheckRoundTrip();
    CheckFirstExitCreatesFile();
    CheckAllZeroGuard();
    CheckWinModeAbsentTakesDefaults();
    CheckGamepadRoundTrip();

    RemoveCfg();

    if (fails == 0) {
        std::printf("test_input_config: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_input_config: %d failure(s)\n", fails);
    return 1;
}
