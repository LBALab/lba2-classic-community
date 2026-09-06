#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define TABKEYS_NUM_KEYS (256 + (16 * 8))

// --- Public state ------------------------------------------------------------
extern S32 AsciiMode;
extern S32 Key;
extern U8 TabKeys[TABKEYS_NUM_KEYS]; ///< Keyboard and Joysticks state

/// 1 = the most recent user input was a finger on the touchscreen; 0 = any other
/// device. Defined in KEYBOARD.CPP beside LastInputWasKeyboard, which answers a
/// different question ("can we prompt for typed text") and cannot tell a pad from
/// a finger, because both leave it 0. The touch overlay needs that distinction to
/// stand down for a gamepad and come back for a finger.
///
/// Deliberately not cleared by the mouse: SDL synthesises mouse events from touch,
/// so a mouse-side clear would cancel every touch the instant it arrived.
extern S32 LastInputWasTouch;

// --- Initialization ----------------------------------------------------------
void InitKeyboard();
void EndKeyboard();

// --- Interface ---------------------------------------------------------------
S32 GetAscii();
S32 CheckKey(U32 key);
void GetKeyScancodeName(char *string, U8 strMaxLen, U32 scanCode);

void ManageKeyboard();
void HandleEventsKeyboard(const void *event);

// =============================================================================
#ifdef __cplusplus
}
#endif
