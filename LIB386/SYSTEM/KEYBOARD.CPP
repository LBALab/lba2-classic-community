#include <SYSTEM/KEYBOARD.H>

#include <SYSTEM/EVENTS.H>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define KeyBit(x) ((x) < 256 ? 0x80 : (1 << ((x)&7)))
#define KeyIndex(x) ((x) < 256 ? (x) : 256 - 32 + ((x) >> 3))

// --- Public state ------------------------------------------------------------
S32 AsciiMode = FALSE;
S32 Key;
U8 TabKeys[TABKEYS_NUM_KEYS];

// --- Private functions -------------------------------------------------------
void UpdateKeyboardState() {
  ManageEvents();

  Key = 0;
  memset(TabKeys, 0, 256);

  SDL_PumpEvents();
  const U8 *keystate = SDL_GetKeyboardState(NULL);
  for (int keyIndex = 0; keyIndex < SDL_NUM_SCANCODES; keyIndex++) {
    if (keystate[keyIndex]) {
      TabKeys[KeyIndex(keyIndex)] |= KeyBit(keyIndex);

      if (Key != 0) {
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
  static S32 lastReturnedAscii = 0;

  ManageEvents();

  const U8 *keystate = SDL_GetKeyboardState(NULL);
  for (int keyIndex = 0; keyIndex < SDL_NUM_SCANCODES; keyIndex++) {
    if (keystate[keyIndex]) {
      S32 asciiValue = SDL_GetKeyFromScancode((SDL_Scancode)keyIndex);

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
                      : TabKeys[256 - 32 + ((key) >> 3)] & (1 << ((key)&7)));
}

void GetKeyScancodeName(char *string, U8 strMaxLen, U32 scanCode) {
  const char *name = SDL_GetScancodeName((SDL_Scancode)scanCode);
  snprintf(string, strMaxLen, "%s", name);
}

void ManageKeyboard() {
  UpdateKeyboardState();
}

void HandleEventsKeyboard(const void *event) {
  // Empty
  // New implementation using SDL does not make use of events, but this may
  //  change in the future
}

// =============================================================================
#ifdef __cplusplus
}
#endif
