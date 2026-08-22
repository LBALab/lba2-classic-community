/* Host-only test for GetAscii's source of characters.
 *
 * GetAscii (LIB386/SYSTEM/KEYBOARD.CPP) answers with the character behind the
 * poll's first held key. What makes that worth a test is where it reads the key
 * from: the polled sample, `Key`, rather than the device. Everything that puts
 * input into this engine without a device writes there -- the touch overlay, the
 * control harness, a replayed recording -- so a reader that goes to SDL instead
 * is invisible to all three, and the screens that read characters are the
 * screens a replay cannot drive.
 *
 * Injection here is the real hook rather than a stand-in: ApplyHarnessKeys is
 * weak in KEYBOARD.CPP, and the strong definition below is exactly what
 * SOURCES/CONTROL.CPP provides in the engine, down to taking `Key` when nothing
 * else has.
 *
 * The keycode is asserted against SDL's own conversion rather than a literal,
 * because the conversion is layout-dependent and the contract is that GetAscii
 * performs it, not that a given scancode is a given letter.
 */
#include <SYSTEM/EVENTS.H> // ManageEvents (stubbed below)
#include <SYSTEM/INPUT.H>  // Input, GetInput, DefineInputKeys
#include <SYSTEM/KEYBOARD.H>

#include <SDL3/SDL.h>

#include <cstdio>

// --- Link stub: UpdateKeyboardState drains the event queue before it samples,
// and this test has no window to drain one from. ---
void ManageEvents() {}

// --- The injector, on the terms KEYBOARD.CPP offers it: after the SDL scan has
// filled TabKeys and taken Key, before anything reads either. ---
static U32 g_injected = 0;

extern "C" void ApplyHarnessKeys(void) {
    if (g_injected && g_injected < 256) {
        TabKeys[g_injected] |= 0x80;
        if (Key == 0)
            Key = (S32)g_injected;
    }
}

static int fails = 0;

static void expect(S32 got, S32 want, const char *label) {
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s: got %d, want %d\n", label, (int)got, (int)want);
        fails++;
    }
}

/* What every caller does: take the sample, then read the character out of it.
   InputPlayerName does exactly this per iteration; the menu loop's poll is
   MyGetInput's. Split here rather than folded into GetAscii, because a reader
   that takes its own poll consumes a recorded one wherever it is called, and
   the callers already sit inside frames that poll. */
static S32 pollThenGetAscii(void) {
    ManageKeyboard();
    return GetAscii();
}

static SDL_Keycode keycodeOf(U32 scancode) {
    return SDL_GetKeyFromScancode((SDL_Scancode)scancode, SDL_KMOD_NONE, true);
}

int main() {
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SKIP: SDL_Init(EVENTS) failed: %s\n", SDL_GetError());
        return 77; // autotools convention, as tests/automation/lib.sh uses
    }

    const U32 kA = SDL_SCANCODE_A;
    const U32 kB = SDL_SCANCODE_B;

    /* A layout that maps neither letter to a character would make every check
       below pass for the wrong reason. */
    if (keycodeOf(kA) == 0 || keycodeOf(kB) == 0 || keycodeOf(kA) == keycodeOf(kB)) {
        std::fprintf(stderr, "SKIP: this layout does not give A and B distinct keycodes\n");
        SDL_Quit();
        return 77;
    }

    // Nothing held: no character, and the sample says so too.
    g_injected = 0;
    expect(pollThenGetAscii(), 0, "idle poll yields no character");
    expect(Key, 0, "idle poll leaves Key clear");

    // An injected key is a key. This is the whole change: before it, the answer
    // came from SDL and a run with no physical keyboard got 0 here forever.
    g_injected = kA;
    expect(pollThenGetAscii(), (S32)keycodeOf(kA), "injected key yields its character");

    /* The poll ran rather than the answer coming from somewhere stale: the
       injected scancode is in this poll's table and in this poll's Key.
       The second of those is also what makes the answer usable next to `Key`,
       which CHEATCOD.CPP stores beside the character it just took. Those two
       did not have to agree before: one came from the poll and the other from a
       separate read of the device. */
    expect(CheckKey(kA) ? 1 : 0, 1, "the poll carries the injected key");
    expect(Key, (S32)kA, "the poll's Key is the injected scancode");

    // Held, not struck again: one character per press.
    g_injected = kA;
    expect(pollThenGetAscii(), 0, "a held key yields nothing further");
    expect(pollThenGetAscii(), 0, "still nothing while it stays down");

    // A different key is a fresh press even with no gap between them.
    g_injected = kB;
    expect(pollThenGetAscii(), (S32)keycodeOf(kB), "a different key yields its character");

    // Released and struck again: the edge is re-armed by the empty poll.
    g_injected = 0;
    expect(pollThenGetAscii(), 0, "release yields nothing");
    g_injected = kA;
    expect(pollThenGetAscii(), (S32)keycodeOf(kA), "the same key struck again yields its character");

    /* Taking the poll must not put anything back that a caller upstream took
       away. SOURCES/INPUT.CPP clears Key, TabKeys and Input while the console is
       open, so that typing at it cannot reach the game, and GetAscii samples
       again after that -- but it rebuilds no action bits, and the bits are what
       the hero is driven by. So the key comes back and the movement does not. */
    {
        U32 keys[1] = {kA};
        U32 masks[1] = {0x1};

        DefineInputKeys(1, keys, masks);

        g_injected = 0;
        pollThenGetAscii(); // re-arm the edge
        g_injected = kA;

        GetInput(0);
        expect((S32)Input, 1, "the funnel does turn that key into an action bit");

        Input = 0; // what MyGetInput does with the console open
        pollThenGetAscii();
        expect((S32)Input, 0, "reading a character rebuilds no action bits");
        expect(CheckKey(kA) ? 1 : 0, 1, "...though it does sample the key again");

        DefineInputKeys(0, NULL, NULL);
    }

    SDL_Quit();

    if (fails == 0) {
        std::printf("test_getascii: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_getascii: %d failure(s)\n", fails);
    return 1;
}
