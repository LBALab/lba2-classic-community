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
#include <SYSTEM/INPUT.H> // Input, GetInput, ClearNoRepeatInput
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H> // K_*
#include <SYSTEM/TIMER.H>

#include "INPUT.H" // SOURCES/INPUT.H
#include "MENU_INPUT.H"

#include "held_keys.h"

extern int TestConsoleOpen;
extern int TestConsoleSuppress;
extern S32 TestTouchGameplay;

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
    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
        DefKeys[n].Key1 = (U32)(100 + n);
        DefKeys[n].Key2 = (U32)(200 + n);
    }
}

void ScribbleBindings(void) {
    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
        DefKeys[n].Key1 = 0xDEAD;
        DefKeys[n].Key2 = 0xBEEF;
    }
}

void ScribblePad(void) {
    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
        GamepadKeys[n].Key1 = 0xDEAD;
        GamepadKeys[n].Key2 = 0xBEEF;
    }
}

bool PadIsDefault(void) {
    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
        if (GamepadKeys[n].Key1 != GamepadKeysDefault[n].Key1 ||
            GamepadKeys[n].Key2 != GamepadKeysDefault[n].Key2)
            return false;
    }
    return true;
}

bool BindingsAreProbeLayout(void) {
    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
        if (DefKeys[n].Key1 != (U32)(100 + n) || DefKeys[n].Key2 != (U32)(200 + n))
            return false;
    }
    return true;
}

// Compares against the defaults without leaving either live table disturbed:
// RestoreInput writes both, and later checks in the same case still read them.
bool BindingsAreDefaults(void) {
    T_DEF_KEY savedKeys[MAX_INPUT_SLOTS];
    T_DEF_KEY savedPad[MAX_INPUT_SLOTS];
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
    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
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
    for (int n = 0; n < MAX_INPUT_SLOTS; n++)
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
    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
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
    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
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
    for (int n = 0; n < MAX_INPUT_SLOTS; n++)
        oneKey += Line("Input%d_1: %u\nInput%d_2: 0\n", n,
                       n == 7 ? (U32)K_SPACE : 0u, n);
    LoadCfg(oneKey + ClearedPad(-1));
    CHECK(DefKeys[7].Key1 == (U32)K_SPACE && DefKeys[0].Key1 == 0,
          "one surviving keyboard binding is enough to keep the cfg");
}

// --- 4. No WinMode ----------------------------------------------------------
void CheckWinModeAbsentTakesDefaults(void) {
    std::string noWinMode;
    for (int n = 0; n < MAX_INPUT_SLOTS; n++)
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

    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
        GamepadKeys[n].Key1 = (U32)(300 + n);
        GamepadKeys[n].Key2 = (U32)(400 + n);
    }
    WriteGamepadConfig();

    ScribblePad();
    ReadGamepadConfig();

    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
        CHECK(GamepadKeys[n].Key1 == (U32)(300 + n) &&
                  GamepadKeys[n].Key2 == (U32)(400 + n),
              "slot %d: pad binding did not survive the round trip", n);
    }
}

void ResetMenuInput(void) {
    RestoreInput();
    InitInput();
    ClearNoRepeatInput();
    MenuInput_Reset(0);
    HoldNone();
    MyGetInput();
}

void CheckMenuDirections(void) {
    ResetMenuInput();
    const U32 keyboard[4][2] = {
        {K_UP, K_NUMPAD_8},
        {K_DOWN, K_NUMPAD_2},
        {K_LEFT, K_NUMPAD_4},
        {K_RIGHT, K_NUMPAD_6},
    };
    const U32 gamepad[4][2] = {
        {K_GAMEPAD_LSTICK_UP, K_GAMEPAD_DPAD_UP},
        {K_GAMEPAD_LSTICK_DOWN, K_GAMEPAD_DPAD_DOWN},
        {K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_DPAD_LEFT},
        {K_GAMEPAD_LSTICK_RIGHT, K_GAMEPAD_DPAD_RIGHT},
    };
    for (int direction = 0; direction < MENU_INPUT_COUNT; direction++) {
        for (int alternate = 0; alternate < 2; alternate++) {
            HoldOnly(keyboard[direction][alternate]);
            MyGetInput();
            CHECK(MenuInput_Get() == (1u << direction),
                  "keyboard direction %d alternate %d", direction, alternate);
            HoldOnly(gamepad[direction][alternate]);
            MyGetInput();
            CHECK(MenuInput_Get() == (1u << direction),
                  "gamepad direction %d alternate %d", direction, alternate);
        }
    }
    GamepadKeys[0].Key1 = K_GAMEPAD_RTRIGGER;
    GamepadKeys[1].Key1 = K_GAMEPAD_LTRIGGER;
    InitInput();
    HoldOnly(K_GAMEPAD_RTRIGGER);
    MyGetInput();
    CHECK((Input & I_UP) && !MenuInput_Get(), "RT moves forward, never navigates UI");
    HoldOnly(K_GAMEPAD_LTRIGGER);
    MyGetInput();
    CHECK((Input & I_DOWN) && !MenuInput_Get(), "LT moves backward, never navigates UI");
    HoldNone();
    MyGetInput();
    CHECK(!MenuInput_Get() && !MenuInput_Held(), "release clears UI state");
    U32 random = 0xDEADBEEFu;
    for (int round = 0; round < 50; round++) {
        random = random * 1103515245u + 12345u;
        U32 directions = (random >> 16) & MENU_INPUT_ALL;
        HoldNone();
        for (int n = 0; n < MENU_INPUT_COUNT; n++) {
            if (directions & (1u << n))
                HoldAdd((round & 1) ? keyboard[n][round & 1] : gamepad[n][round & 1]);
        }
        MyGetInput();
        CHECK(MenuInput_Get() == directions, "UI simultaneous directions round %d", round);
    }
}

void CheckMenuRebindIsolation(void) {
    ResetMenuInput();
    DefKeys[0].Key1 = K_Z;
    DefKeys[0].Key2 = 0;
    InitInput();
    HoldOnly(K_Z);
    MyGetInput();
    CHECK((Input & I_UP) && !MenuInput_Get(), "gameplay rebind does not navigate UI");
    HoldOnly(K_UP);
    MyGetInput();
    CHECK(!(Input & I_UP) && MenuInput_Get() == MENU_INPUT_UP,
          "UI arrows survive gameplay rebind");

    GamepadKeys[0].Key1 = K_GAMEPAD_RSTICK_UP;
    GamepadKeys[0].Key2 = 0;
    InitInput();
    HoldOnly(K_Q);
    MyGetInput();
    CHECK(!MenuInput_Get(), "other keyboard keys do not navigate UI");
    HoldOnly(K_GAMEPAD_DPAD_UP);
    MyGetInput();
    CHECK(MenuInput_Get() == MENU_INPUT_UP, "UI D-pad survives gameplay rebind");
    HoldOnly(K_GAMEPAD_RSTICK_UP);
    MyGetInput();
    CHECK((Input & I_UP) && !MenuInput_Get(), "gameplay right-stick rebind does not navigate UI");

    for (int n = 0; n < MAX_INPUT_SLOTS; n++) {
        DefKeys[n].Key1 = DefKeys[n].Key2 = 0;
        GamepadKeys[n].Key1 = GamepadKeys[n].Key2 = 0;
    }
    InitInput();
    const U32 fixedKeys[] = {K_DOWN, K_NUMPAD_4, K_GAMEPAD_LSTICK_UP, K_GAMEPAD_DPAD_RIGHT};
    const U32 directions[] = {MENU_INPUT_DOWN, MENU_INPUT_LEFT, MENU_INPUT_UP, MENU_INPUT_RIGHT};
    const S32 navigation[] = {K_DOWN, K_LEFT, K_UP, K_RIGHT};
    for (int n = 0; n < 4; n++) {
        HoldOnly(fixedKeys[n]);
        MyGetInput();
        CHECK(!Input && MenuInput_Get() == directions[n] &&
                  MenuInput_NavigationKey() == navigation[n],
              "fixed direction %d survives clearing every gameplay binding", n);
    }
}

void CheckMenuSuppression(void) {
    ResetMenuInput();
    HoldOnly(K_GAMEPAD_DPAD_UP);
    MyGetInput();
    MenuInput_Suppress(MENU_INPUT_UP);
    MyGetInput();
    CHECK(!MenuInput_Get() && MenuInput_Held() == MENU_INPUT_UP,
          "suppressed held direction still participates in release waits");
    HoldAdd(K_GAMEPAD_DPAD_RIGHT);
    MyGetInput();
    CHECK(MenuInput_Get() == MENU_INPUT_RIGHT, "suppression is per direction");
    HoldNone();
    MyGetInput();
    HoldOnly(K_UP);
    MyGetInput();
    CHECK(MenuInput_Get() == MENU_INPUT_UP, "release rearms direction");
    TestConsoleOpen = 1;
    MyGetInput();
    CHECK(!MenuInput_Get() && !MenuInput_Held(), "console isolates UI input and waits");
    TestConsoleOpen = 0;
    TestConsoleSuppress = 1;
    MyGetInput();
    CHECK(!MenuInput_Get() && !MenuInput_Held(), "console close frame isolates UI input");
    TestConsoleSuppress = 0;
    MyGetInput();
    CHECK(!MenuInput_Get(), "held console key cannot leak into UI on close");
    HoldNone();
    MyGetInput();
    HoldOnly(K_UP);
    MyGetInput();
    CHECK(MenuInput_Get() == MENU_INPUT_UP, "UI navigation resumes after release");
    HoldOnly(K_GAMEPAD_LSTICK_UP);
    ReleaseAfterPolls(3);
    WaitNoInput();
    CHECK(!MenuInput_Held(), "release wait drains directions absent from gameplay Input");
    MenuInput_Reset(MENU_INPUT_UP);
    HoldOnly(K_UP);
    MyGetInput();
    CHECK(!MenuInput_Get() && MenuInput_Suppressed() == MENU_INPUT_UP,
          "replay restores the recorded UI latch");
    MenuInput_Reset(0);
    MyGetInput();
    CHECK(MenuInput_Get() == MENU_INPUT_UP, "unsuppressed first replay poll is not discarded");
}

void CheckTouchGameplayLifetime(void) {
    ResetMenuInput();
    MyGetGameplayInput();
    CHECK(TestTouchGameplay, "gameplay touch eligibility must survive into rendering");
    MyGetInput();
    CHECK(!TestTouchGameplay, "modal polling must end gameplay touch eligibility");
    TestConsoleOpen = 1;
    MyGetGameplayInput();
    CHECK(!TestTouchGameplay, "console presentation must not queue gameplay touches");
    TestConsoleOpen = 0;
    TestConsoleSuppress = 1;
    MyGetGameplayInput();
    CHECK(!TestTouchGameplay, "console closing frame must discard gameplay touches");
    TestConsoleSuppress = 0;
}

#ifdef LBA2_GAMEPAD_STICK_LAYOUT
void CheckCircleContextRelease(void) {
    const U32 directions[] = {K_GAMEPAD_LSTICK_UP, K_GAMEPAD_LSTICK_DOWN,
                              K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_LSTICK_RIGHT};
    RestoreInput();
    InitInput();
    ClearNoRepeatInput();
    HoldNone();
    MyGetInput();
    for (int direction = 0; direction < 4; ++direction) {
        HoldOnly(K_GAMEPAD_B);
        MyGetInput();
        CHECK(Input == I_RETURN, "fresh Circle press confirms in modal context");
        InitWaitNoInput(I_INVENTORY | I_RETURN | I_MENUS);
        MyGetGameplayInput();
        CHECK(!Input, "held modal Circle must not become gameplay dodge");
        HoldAdd(directions[direction]);
        for (int frame = 0; frame < 3; ++frame) {
            MyGetGameplayInput();
            CHECK(Input == (1u << direction), "Circle remains blocked until physical release");
        }
        HoldAdd(K_X);
        MyGetGameplayInput();
        CHECK(Input == (I_ESQUIVE | (1u << direction)), "Circle suppression must not block keyboard dodge");
        HoldRemove(K_X);
        HoldRemove(K_GAMEPAD_B);
        MyGetGameplayInput();
        CHECK(Input == (1u << direction), "Circle release preserves held movement");
        HoldAdd(K_GAMEPAD_B);
        for (int frame = 0; frame < 3; ++frame) {
            MyGetGameplayInput();
            CHECK(Input == (I_ESQUIVE | (1u << direction)), "fresh Circle dodge remains held across gameplay frames");
        }
        MyGetInput();
        CHECK(Input == (1u << direction), "held gameplay Circle must not confirm a modal");
        HoldNone();
        MyGetInput();
        HoldOnly(K_GAMEPAD_B);
        MyGetInput();
        CHECK(Input == I_RETURN, "Circle release rearms modal confirmation");
        HoldNone();
        MyGetInput();
    }
}
#endif

void CheckMenuIgnoresConfig(void) {
    ResetMenuInput();
    WriteCfg(Line("WinMode: 1\nInput0_1: %u\nInput0_2: 0\n"
                  "MenuInput0_1: %u\nMenuInput0_2: 0\n"
                  "MenuGamepad0_1: %u\nMenuGamepad0_2: 0\n",
                  (U32)K_Q, (U32)K_Q, (U32)K_GAMEPAD_RSTICK_UP));
    OpenCfg();
    ReadInputConfig();
    HoldOnly(K_UP);
    MyGetInput();
    CHECK(MenuInput_Get() == MENU_INPUT_UP, "config cannot replace fixed UI arrows");
    HoldOnly(K_GAMEPAD_DPAD_UP);
    MyGetInput();
    CHECK(MenuInput_Get() == MENU_INPUT_UP, "config cannot replace fixed UI D-pad");
    HoldOnly(K_Q);
    MyGetInput();
    CHECK((Input & I_UP) && !MenuInput_Get(), "only gameplay uses configured movement");
    HoldOnly(K_GAMEPAD_RSTICK_UP);
    MyGetInput();
    CHECK(!MenuInput_Get(), "config cannot bind right stick to UI navigation");

    RemoveCfg();
    OpenCfg();
    WriteInputConfig();
    FILE *f = std::fopen(kCfgPath, "rb");
    CHECK(f != NULL, "gameplay config is written");
    if (f) {
        char contents[64 * 1024];
        const size_t count = std::fread(contents, 1, sizeof contents - 1, f);
        std::fclose(f);
        contents[count] = '\0';
        CHECK(!std::strstr(contents, "MenuInput") && !std::strstr(contents, "MenuGamepad"),
              "writing gameplay bindings adds no UI binding settings");
    }
}

void CheckConsoleMenuReplay(void) {
    const int held[] = {1, 1, 1, 0, 1};
    const int open[] = {1, 0, 0, 0, 0};
    const int closing[] = {0, 1, 0, 0, 0};
    U32 active[5], waiting[5], suppressed[5];
    U8 samples[5];
    ResetMenuInput();
    for (int i = 0; i < 5; ++i) {
        HoldNone();
        if (held[i])
            HoldOnly(K_UP);
        TestConsoleOpen = open[i];
        TestConsoleSuppress = closing[i];
        samples[i] = (U8)MenuInput_Capture(open[i] || closing[i], 0);
        MyGetInput();
        active[i] = MenuInput_Get();
        waiting[i] = MenuInput_Held();
        suppressed[i] = MenuInput_Suppressed();
    }

    TestConsoleOpen = TestConsoleSuppress = 0;
    ResetMenuInput();
    for (int i = 0; i < 5; ++i) {
        HoldNone();
        if (held[i] && !open[i])
            HoldOnly(K_UP);
        MenuInput_SetReplaySample(samples[i]);
        MyGetInput();
        CHECK(MenuInput_Get() == active[i], "console replay active directions at poll %d", i);
        CHECK(MenuInput_Held() == waiting[i], "console replay held directions at poll %d", i);
        CHECK(MenuInput_Suppressed() == suppressed[i], "console replay release latch at poll %d", i);
    }
    MenuInput_SetReplaySample(-1);
    HoldOnly(K_UP);
    CHECK(!(MenuInput_Capture(0, K_UP) & MENU_INPUT_UP), "console toggle is not a UI key");
    HoldAdd(K_NUMPAD_8);
    CHECK(MenuInput_Capture(0, K_UP) & MENU_INPUT_UP, "reserving toggle preserves alternate UI keys");
    HoldNone();
    MyGetInput();
    CHECK(!MenuInput_Get(), "ending replay returns UI sampling to physical keys");
}

static int releasePolls = 0;
static U32 releaseDirection = K_GAMEPAD_DPAD_UP;

void AdvanceReleasePoll(void) {
    Timer_FixedDtAdvance();
    if (++releasePolls == 3)
        HoldRemove(releaseDirection);
    if (releasePolls == 7)
        HoldRemove(K_GAMEPAD_RTRIGGER);
}

void CheckPausedMenuRelease(void) {
    const U32 directions[] = {K_UP, K_GAMEPAD_LSTICK_UP, K_GAMEPAD_DPAD_UP,
                              K_GAMEPAD_DPAD_DOWN, K_GAMEPAD_DPAD_LEFT, K_GAMEPAD_DPAD_RIGHT};
    for (int nested = 0; nested < 2; ++nested) {
        for (unsigned direction = 0; direction < sizeof(directions) / sizeof(directions[0]); ++direction) {
            ResetMenuInput();
            GamepadKeys[0].Key1 = K_GAMEPAD_RTRIGGER;
            InitInput();
            Timer_EnableFixedDt(16);
            Timer_FixedDtAdvance();
            ManageTime();
            const U32 before = TimerRefHR;
            releaseDirection = directions[direction];
            HoldOnly(releaseDirection);
            HoldAdd(K_GAMEPAD_RTRIGGER);
            MyGetInput();
            if (nested)
                SaveTimer();
            releasePolls = 0;
            SetPollHook(AdvanceReleasePoll);
            WaitNoMenuInput();
            SetPollHook(NULL);
            CHECK(CmptMemoTimerRef == nested, "UI release wait balances timer depth %d", nested);
            if (nested)
                RestoreTimer();
            ManageTime();
            CHECK(TimerRefHR == before, "UI release wait must not advance gameplay time");
            CHECK(releasePolls == 3, "UI release wait must ignore held RT");
            CHECK(Input & I_UP, "RT remains held after UI release");
            CHECK(!MenuInput_Held(), "UI direction is released before returning");
            CHECK(!CheckKey(releaseDirection), "UI direction cannot reach raw gameplay spell checks");
        }
    }
    HoldNone();
    MyGetInput();
    releasePolls = 0;
    SetPollHook(AdvanceReleasePoll);
    WaitNoMenuInput();
    SetPollHook(NULL);
    CHECK(releasePolls == 0, "no UI direction means no extra input poll");
    Timer_DisableFixedDt();
}

} // namespace

int main() {
    CheckRoundTrip();
    CheckFirstExitCreatesFile();
    CheckAllZeroGuard();
    CheckWinModeAbsentTakesDefaults();
    CheckGamepadRoundTrip();
    CheckMenuDirections();
    CheckMenuRebindIsolation();
    CheckMenuSuppression();
    CheckTouchGameplayLifetime();
#ifdef LBA2_GAMEPAD_STICK_LAYOUT
    CheckCircleContextRelease();
#endif
    CheckMenuIgnoresConfig();
    CheckConsoleMenuReplay();
    CheckPausedMenuRelease();

    RemoveCfg();

    if (fails == 0) {
        std::printf("test_input_config: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_input_config: %d failure(s)\n", fails);
    return 1;
}
