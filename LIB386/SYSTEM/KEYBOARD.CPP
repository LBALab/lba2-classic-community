#include <SYSTEM/KEYBOARD.H>

#include <SYSTEM/EVENTS.H>

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

// Last-input-wins device flag, read by the save menu to skip text entry on
// gamepad-only setups. Declared extern in SOURCES/C_EXTERN.H. Defined here
// (rather than SOURCES/GLOBAL.CPP) so the symbol lives in libsys.a, which
// every test binary links; otherwise tests that pull libsys.a but not the
// SOURCES objects fail to link the HandleEventsXxx consumers below.
S32 LastInputWasGamepad = 0;

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
    // is sample SDL_EVENT_KEY_DOWN to maintain the "last input was gamepad"
    // signal: a real key-down is unambiguous evidence the user is on the
    // keyboard, in a way that polled state isn't (polled state reads "down"
    // on every frame a held key is depressed and would constantly fight any
    // gamepad signal during normal play).
    const SDL_Event *ev = (const SDL_Event *)event;
    if (ev->type == SDL_EVENT_KEY_DOWN) {
        LastInputWasGamepad = 0;
    }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
