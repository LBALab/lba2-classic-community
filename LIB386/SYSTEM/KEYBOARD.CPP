#include <SYSTEM/KEYBOARD.H>

#include <SYSTEM/EVENTS.H>
#include <SYSTEM/INPUT_FLOW.H>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define KeyBit(x) ((x) < 256 ? 0x80 : (1 << ((x) & 7)))
#define KeyIndex(x) ((x) < 256 ? (x) : 256 - 32 + ((x) >> 3))

// --- Public state ------------------------------------------------------------
S32 AsciiMode = FALSE;
S32 Key;
U8 TabKeys[TABKEYS_NUM_KEYS];

// Last-input-wins device flag: 1 = the most recent input was a physical
// keyboard key-down, 0 = anything else (mouse, gamepad, touch). Read by the
// save menu to offer text entry only when a real keyboard is in use. Declared
// extern in SOURCES/C_EXTERN.H. Defined here (rather than SOURCES/GLOBAL.CPP)
// so the symbol lives in libsys.a, which every test binary links; otherwise
// tests that pull libsys.a but not the SOURCES objects fail to link the
// HandleEventsXxx consumers below.
S32 LastInputWasKeyboard = 0;

// Weak hook for virtual-input sources (touch overlay, etc.) to re-apply
// their key states after UpdateKeyboardState clears TabKeys[0..255].
void ApplyVirtualKeys(void);
__attribute__((weak)) void ApplyVirtualKeys(void) {}

// Weak hook for the control harness to hold a key down, on the same terms.
// Separate from ApplyVirtualKeys so the touch overlay and the harness do not
// have to know about each other; both run before the CheckKey scan below, so
// either can put a key into this frame's state.
//
// This is the only injection point that a modal loop sees. The `input` timeline
// ORs its mask into Input from MainLoop (PERSO.CPP), which a menu or dialogue
// spinning in its own MyGetInput loop never reaches; every one of those loops
// does pass through here.
//
// Keys are what it was named for and no longer all it carries: the harness's
// mouse and right stick ride the same hook, because they want the same clock
// (one application per poll) and the same window (after the devices have been
// sampled and cleared, before anything reads them). Kept under the old name
// rather than renamed, since the name is quoted across the input plans and the
// recording research as the seam they were written against.
void ApplyHarnessKeys(void);
__attribute__((weak)) void ApplyHarnessKeys(void) {}

/* Session recording (docs/RECORDING.md). Weak so libsys.a
   still links into the host tests, which have no recorder. */
void Record_PollHook(void);
__attribute__((weak)) void Record_PollHook(void) {}

// --- Private functions -------------------------------------------------------
void UpdateKeyboardState() {
    ManageEvents();

    Key = 0;
    memset(TabKeys, 0, 256);

    SDL_PumpEvents();
    const bool *keystate = SDL_GetKeyboardState(NULL);
    for (int keyIndex = 0; keyIndex < SDL_SCANCODE_COUNT; keyIndex++) {
        if (keystate[keyIndex]) {
            TabKeys[KeyIndex(keyIndex)] |= KeyBit(keyIndex);

            if (Key == 0) {
                Key = keyIndex;
            }
        }
    }

    /* Boundary 1's state side, taken here rather than after the two hooks
       below: they put keys into TabKeys that no SDL event produced, and those
       would read as presses the event side had lost. */
    InputFlow_NotePollSampled(TabKeys);

    ApplyVirtualKeys();
    ApplyHarnessKeys();
    /* The session recorder, on the same terms as the two hooks above. */
    Record_PollHook();

    /* Boundary 2's key side, after the hooks: an injected key is a real key as
       far as the binding table is concerned. That includes a replayed one, which
       is why this comes after the recorder rather than before it: a replay puts
       the file's keys here, and the count has to be of what the game will read. */
    InputFlow_NotePollFinal(TabKeys);
}

// --- Initialization ----------------------------------------------------------
void InitKeyboard() {
    // Empty
}

void EndKeyboard() {
    // Empty
}

// --- Interface ---------------------------------------------------------------
S32 GetAscii() {
    static SDL_Keycode lastReturnedAscii = 0;

    ManageEvents();

    const bool *keystate = SDL_GetKeyboardState(NULL);
    for (int keyIndex = 0; keyIndex < SDL_SCANCODE_COUNT; keyIndex++) {
        if (keystate[keyIndex]) {
            SDL_Keycode asciiValue = SDL_GetKeyFromScancode((SDL_Scancode)keyIndex, SDL_KMOD_NONE, true);

            if (lastReturnedAscii == asciiValue) {
                return 0;
            }

            lastReturnedAscii = asciiValue;
            return asciiValue;
        }
    }

    lastReturnedAscii = 0;

    return 0;
}

S32 CheckKey(U32 key) {
    return ((key) < 256 ? TabKeys[(key)] & 0x80
                        : TabKeys[256 - 32 + ((key) >> 3)] & (1 << ((key) & 7)));
}

void GetKeyScancodeName(char *string, U8 strMaxLen, U32 scanCode) {
    const char *name = SDL_GetScancodeName((SDL_Scancode)scanCode);
    snprintf(string, strMaxLen, "%s", name);
}

void ManageKeyboard() {
    UpdateKeyboardState();
}

void HandleEventsKeyboard(const void *event) {
    // Keyboard *state* is read via SDL_GetKeyboardState in UpdateKeyboardState;
    // we don't consume key events to drive TabKeys. The one thing we do here
    // is sample SDL_EVENT_KEY_DOWN to maintain the "last input was keyboard"
    // signal: a real key-down is unambiguous evidence the user is on a
    // physical keyboard, in a way that polled state isn't (polled state reads
    // "down" on every frame a held key is depressed). The touch overlay pokes
    // TabKeys directly and never reaches here, so touch never counts as
    // keyboard.
    const SDL_Event *ev = (const SDL_Event *)event;
    if (ev->type == SDL_EVENT_KEY_DOWN) {
        LastInputWasKeyboard = 1;
    }

    /* Boundary 1's event side. Auto-repeat is excluded: a held key repeats at
       the desktop's rate and none of those repeats is a press, so counting them
       would swamp the one number this boundary exists to produce. */
    if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat)
        InputFlow_NoteKeyEvent(TRUE);
    else if (ev->type == SDL_EVENT_KEY_UP)
        InputFlow_NoteKeyEvent(FALSE);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
