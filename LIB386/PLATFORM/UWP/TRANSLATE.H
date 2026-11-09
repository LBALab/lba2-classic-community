#pragma once

/* The UWP backend's translations into the engine's vocabulary, kept free of
   Windows headers so host tests on any OS can check them. Numbers that come
   from Windows (virtual keys, GamepadButtons bits) are spelled out here. */

#include <PLATFORM/INPUT.H>

#include <stdint.h>

/* --- Keyboard ------------------------------------------------------------ */

/* Set-1 scan codes as CorePhysicalKeyStatus reports them, to physical key IDs. */
static const uint8_t kUwpScanBase[0x59] = {
    0, PLATFORM_SCANCODE_ESCAPE,
    PLATFORM_SCANCODE_1, PLATFORM_SCANCODE_2, PLATFORM_SCANCODE_3, PLATFORM_SCANCODE_4,
    PLATFORM_SCANCODE_5, PLATFORM_SCANCODE_6, PLATFORM_SCANCODE_7, PLATFORM_SCANCODE_8,
    PLATFORM_SCANCODE_9, PLATFORM_SCANCODE_0, PLATFORM_SCANCODE_MINUS, PLATFORM_SCANCODE_EQUALS,
    PLATFORM_SCANCODE_BACKSPACE, PLATFORM_SCANCODE_TAB,
    /* 0x10 */
    PLATFORM_SCANCODE_Q, PLATFORM_SCANCODE_W, PLATFORM_SCANCODE_E, PLATFORM_SCANCODE_R,
    PLATFORM_SCANCODE_T, PLATFORM_SCANCODE_Y, PLATFORM_SCANCODE_U, PLATFORM_SCANCODE_I,
    PLATFORM_SCANCODE_O, PLATFORM_SCANCODE_P, PLATFORM_SCANCODE_LEFTBRACKET, PLATFORM_SCANCODE_RIGHTBRACKET,
    PLATFORM_SCANCODE_RETURN, PLATFORM_SCANCODE_LCTRL, PLATFORM_SCANCODE_A, PLATFORM_SCANCODE_S,
    /* 0x20 */
    PLATFORM_SCANCODE_D, PLATFORM_SCANCODE_F, PLATFORM_SCANCODE_G, PLATFORM_SCANCODE_H,
    PLATFORM_SCANCODE_J, PLATFORM_SCANCODE_K, PLATFORM_SCANCODE_L, PLATFORM_SCANCODE_SEMICOLON,
    PLATFORM_SCANCODE_APOSTROPHE, PLATFORM_SCANCODE_GRAVE, PLATFORM_SCANCODE_LSHIFT, PLATFORM_SCANCODE_BACKSLASH,
    PLATFORM_SCANCODE_Z, PLATFORM_SCANCODE_X, PLATFORM_SCANCODE_C, PLATFORM_SCANCODE_V,
    /* 0x30 */
    PLATFORM_SCANCODE_B, PLATFORM_SCANCODE_N, PLATFORM_SCANCODE_M, PLATFORM_SCANCODE_COMMA,
    PLATFORM_SCANCODE_PERIOD, PLATFORM_SCANCODE_SLASH, PLATFORM_SCANCODE_RSHIFT, PLATFORM_SCANCODE_KP_MULTIPLY,
    PLATFORM_SCANCODE_LALT, PLATFORM_SCANCODE_SPACE, PLATFORM_SCANCODE_CAPSLOCK, PLATFORM_SCANCODE_F1,
    PLATFORM_SCANCODE_F2, PLATFORM_SCANCODE_F3, PLATFORM_SCANCODE_F4, PLATFORM_SCANCODE_F5,
    /* 0x40 */
    PLATFORM_SCANCODE_F6, PLATFORM_SCANCODE_F7, PLATFORM_SCANCODE_F8, PLATFORM_SCANCODE_F9,
    PLATFORM_SCANCODE_F10, PLATFORM_SCANCODE_PAUSE, PLATFORM_SCANCODE_SCROLLLOCK, PLATFORM_SCANCODE_KP_7,
    PLATFORM_SCANCODE_KP_8, PLATFORM_SCANCODE_KP_9, PLATFORM_SCANCODE_KP_MINUS, PLATFORM_SCANCODE_KP_4,
    PLATFORM_SCANCODE_KP_5, PLATFORM_SCANCODE_KP_6, PLATFORM_SCANCODE_KP_PLUS, PLATFORM_SCANCODE_KP_1,
    /* 0x50 */
    PLATFORM_SCANCODE_KP_2, PLATFORM_SCANCODE_KP_3, PLATFORM_SCANCODE_KP_0, PLATFORM_SCANCODE_KP_PERIOD,
    0, 0, PLATFORM_SCANCODE_NONUSBACKSLASH, PLATFORM_SCANCODE_F11,
    PLATFORM_SCANCODE_F12};

/* The same codes with the E0 prefix (IsExtendedKey). */
static inline int32_t UwpExtendedScan(uint32_t scan) {
    switch (scan) {
    case 0x1C:
        return PLATFORM_SCANCODE_KP_ENTER;
    case 0x1D:
        return PLATFORM_SCANCODE_RCTRL;
    case 0x35:
        return PLATFORM_SCANCODE_KP_DIVIDE;
    case 0x37:
        return PLATFORM_SCANCODE_PRINTSCREEN;
    case 0x38:
        return PLATFORM_SCANCODE_RALT;
    case 0x45:
        return PLATFORM_SCANCODE_NUMLOCKCLEAR;
    case 0x47:
        return PLATFORM_SCANCODE_HOME;
    case 0x48:
        return PLATFORM_SCANCODE_UP;
    case 0x49:
        return PLATFORM_SCANCODE_PAGEUP;
    case 0x4B:
        return PLATFORM_SCANCODE_LEFT;
    case 0x4D:
        return PLATFORM_SCANCODE_RIGHT;
    case 0x4F:
        return PLATFORM_SCANCODE_END;
    case 0x50:
        return PLATFORM_SCANCODE_DOWN;
    case 0x51:
        return PLATFORM_SCANCODE_PAGEDOWN;
    case 0x52:
        return PLATFORM_SCANCODE_INSERT;
    case 0x53:
        return PLATFORM_SCANCODE_DELETE;
    case 0x5B:
        return PLATFORM_SCANCODE_LGUI;
    case 0x5C:
        return PLATFORM_SCANCODE_RGUI;
    case 0x5D:
        return PLATFORM_SCANCODE_APPLICATION;
    default:
        return PLATFORM_SCANCODE_UNKNOWN;
    }
}

/* Keys that arrive without a scan code (an on-screen keyboard, a remote) are
   placed by their virtual key, for the keys the game reads. */
static inline int32_t UwpVirtualKeyScan(uint32_t vk) {
    if (vk >= 0x41 && vk <= 0x5A) /* A..Z */
        return PLATFORM_SCANCODE_A + (int32_t)(vk - 0x41);
    if (vk >= 0x31 && vk <= 0x39) /* 1..9 */
        return PLATFORM_SCANCODE_1 + (int32_t)(vk - 0x31);
    if (vk >= 0x70 && vk <= 0x7B) /* F1..F12 */
        return PLATFORM_SCANCODE_F1 + (int32_t)(vk - 0x70);
    switch (vk) {
    case 0x30:
        return PLATFORM_SCANCODE_0;
    case 0x08:
        return PLATFORM_SCANCODE_BACKSPACE;
    case 0x09:
        return PLATFORM_SCANCODE_TAB;
    case 0x0D:
        return PLATFORM_SCANCODE_RETURN;
    case 0x1B:
        return PLATFORM_SCANCODE_ESCAPE;
    case 0x20:
        return PLATFORM_SCANCODE_SPACE;
    case 0x21:
        return PLATFORM_SCANCODE_PAGEUP;
    case 0x22:
        return PLATFORM_SCANCODE_PAGEDOWN;
    case 0x23:
        return PLATFORM_SCANCODE_END;
    case 0x24:
        return PLATFORM_SCANCODE_HOME;
    case 0x25:
        return PLATFORM_SCANCODE_LEFT;
    case 0x26:
        return PLATFORM_SCANCODE_UP;
    case 0x27:
        return PLATFORM_SCANCODE_RIGHT;
    case 0x28:
        return PLATFORM_SCANCODE_DOWN;
    case 0x2D:
        return PLATFORM_SCANCODE_INSERT;
    case 0x2E:
        return PLATFORM_SCANCODE_DELETE;
    case 0x10:
    case 0xA0:
        return PLATFORM_SCANCODE_LSHIFT;
    case 0xA1:
        return PLATFORM_SCANCODE_RSHIFT;
    case 0x11:
    case 0xA2:
        return PLATFORM_SCANCODE_LCTRL;
    case 0xA3:
        return PLATFORM_SCANCODE_RCTRL;
    case 0x12:
    case 0xA4:
        return PLATFORM_SCANCODE_LALT;
    case 0xA5:
        return PLATFORM_SCANCODE_RALT;
    default:
        return PLATFORM_SCANCODE_UNKNOWN;
    }
}

/* The Xbox also turns gamepad buttons into key events: the navigation keys
   (0x88-0x8F) and the gamepad keys (0xC3-0xDA). The pad is read directly, so
   these are dropped rather than pressed twice. */
static inline bool UwpIsGamepadVirtualKey(uint32_t vk) {
    return (vk >= 0x88 && vk <= 0x8F) || (vk >= 0xC3 && vk <= 0xDA);
}

static inline int32_t UwpKeyScancode(uint32_t vk, uint32_t scan, bool extended) {
    if (UwpIsGamepadVirtualKey(vk))
        return PLATFORM_SCANCODE_UNKNOWN;
    /* Pause sends 0x45 like Num Lock, which differs only by the E0 prefix; the
       virtual key tells them apart for certain. */
    if (vk == 0x13)
        return PLATFORM_SCANCODE_PAUSE;
    if (vk == 0x90)
        return PLATFORM_SCANCODE_NUMLOCKCLEAR;
    int32_t code = PLATFORM_SCANCODE_UNKNOWN;
    if (extended)
        code = UwpExtendedScan(scan);
    else if (scan < sizeof(kUwpScanBase))
        code = kUwpScanBase[scan];
    return code != PLATFORM_SCANCODE_UNKNOWN ? code : UwpVirtualKeyScan(vk);
}

static inline uint16_t UwpModifierBit(int32_t scancode) {
    switch (scancode) {
    case PLATFORM_SCANCODE_LSHIFT:
        return PLATFORM_KMOD_LSHIFT;
    case PLATFORM_SCANCODE_RSHIFT:
        return PLATFORM_KMOD_RSHIFT;
    case PLATFORM_SCANCODE_LCTRL:
        return PLATFORM_KMOD_LCTRL;
    case PLATFORM_SCANCODE_RCTRL:
        return PLATFORM_KMOD_RCTRL;
    case PLATFORM_SCANCODE_LALT:
        return PLATFORM_KMOD_LALT;
    case PLATFORM_SCANCODE_RALT:
        return PLATFORM_KMOD_RALT;
    case PLATFORM_SCANCODE_LGUI:
        return PLATFORM_KMOD_LGUI;
    case PLATFORM_SCANCODE_RGUI:
        return PLATFORM_KMOD_RGUI;
    default:
        return 0;
    }
}

/* Key code on a US layout, as SDL gives it: the character for printable keys,
   shifted when Shift is held, and the scan code with the keycode bit for the
   rest. The game asks for characters through text input instead; this serves
   key bindings and the console's Ctrl shortcuts. */
static inline uint32_t UwpKeycode(int32_t scancode, uint16_t mods) {
    static const char kPlain[] = "1234567890\r\x1b\b\t -=[]\\#;'`,./";
    static const char kShift[] = "!@#$%^&*()\r\x1b\b\t _+{}|~:\"~<>?";
    const bool shift = (mods & PLATFORM_KMOD_SHIFT) != 0;
    if (scancode >= PLATFORM_SCANCODE_A && scancode <= PLATFORM_SCANCODE_Z)
        return (uint32_t)((shift ? 'A' : 'a') + (scancode - PLATFORM_SCANCODE_A));
    if (scancode >= PLATFORM_SCANCODE_1 && scancode <= PLATFORM_SCANCODE_SLASH)
        return (uint32_t)(unsigned char)(shift ? kShift : kPlain)[scancode - PLATFORM_SCANCODE_1];
    if (scancode == PLATFORM_SCANCODE_DELETE)
        return PLATFORM_KEYCODE_DELETE;
    /* The key beside left Shift on ISO keyboards has no US character. */
    if (scancode == PLATFORM_SCANCODE_NONUSBACKSLASH)
        return 0;
    if (scancode <= PLATFORM_SCANCODE_UNKNOWN || scancode >= PLATFORM_SCANCODE_COUNT)
        return 0;
    return PLATFORM_SCANCODE_TO_KEYCODE(scancode);
}

/* Key names as SDL spells them, which is what the key binding screens show. */
static inline const char *UwpScancodeName(int32_t scancode) {
    static const char *const kLetters[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                                           "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};
    static const char *const kRow[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
                                       "Return", "Escape", "Backspace", "Tab", "Space", "-", "=", "[", "]", "\\",
                                       "#", ";", "'", "`", ",", ".", "/", "CapsLock",
                                       "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
                                       "PrintScreen", "ScrollLock", "Pause", "Insert", "Home", "PageUp", "Delete",
                                       "End", "PageDown", "Right", "Left", "Down", "Up", "Numlock",
                                       "Keypad /", "Keypad *", "Keypad -", "Keypad +", "Keypad Enter",
                                       "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4", "Keypad 5",
                                       "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9", "Keypad 0", "Keypad .",
                                       "NonUSBackslash", "Application"};
    static const char *const kMods[] = {"Left Ctrl", "Left Shift", "Left Alt", "Left GUI",
                                        "Right Ctrl", "Right Shift", "Right Alt", "Right GUI"};
    if (scancode >= PLATFORM_SCANCODE_A && scancode <= PLATFORM_SCANCODE_Z)
        return kLetters[scancode - PLATFORM_SCANCODE_A];
    if (scancode >= PLATFORM_SCANCODE_1 && scancode <= PLATFORM_SCANCODE_APPLICATION)
        return kRow[scancode - PLATFORM_SCANCODE_1];
    if (scancode >= PLATFORM_SCANCODE_LCTRL && scancode <= PLATFORM_SCANCODE_RGUI)
        return kMods[scancode - PLATFORM_SCANCODE_LCTRL];
    return "";
}

/* --- Gamepad ------------------------------------------------------------- */

/* Windows.Gaming.Input.GamepadButtons bits, in PlatformGamepadButton order.
   The Xbox pad has no guide button an app can read. */
static const uint32_t kUwpPadButtonBits[PLATFORM_GAMEPAD_BUTTON_COUNT] = {
    0x0004, /* SOUTH: A */
    0x0008, /* EAST: B */
    0x0010, /* WEST: X */
    0x0020, /* NORTH: Y */
    0x0002, /* BACK: View */
    0x0000, /* GUIDE */
    0x0001, /* START: Menu */
    0x1000, /* LEFT_STICK */
    0x2000, /* RIGHT_STICK */
    0x0400, /* LEFT_SHOULDER */
    0x0800, /* RIGHT_SHOULDER */
    0x0040, /* DPAD_UP */
    0x0080, /* DPAD_DOWN */
    0x0100, /* DPAD_LEFT */
    0x0200  /* DPAD_RIGHT */
};

static inline bool UwpPadButton(uint32_t buttons, PlatformGamepadButton button) {
    if (button < 0 || button >= PLATFORM_GAMEPAD_BUTTON_COUNT)
        return false;
    const uint32_t bit = kUwpPadButtonBits[button];
    return bit != 0 && (buttons & bit) != 0;
}

/* A stick reads -1..1 with Y up; the engine wants -32768..32767 with Y down.
   A trigger reads 0..1; the engine wants 0..32767. */
static inline int16_t UwpPadUnit(double value, bool negate) {
    if (negate)
        value = -value;
    if (value != value) /* NaN */
        return 0;
    if (value >= 1.0)
        return 32767;
    if (value <= -1.0)
        return -32768;
    return (int16_t)(value * (value < 0 ? 32768.0 : 32767.0));
}

/* --- Presentation -------------------------------------------------------- */

typedef struct UwpRect {
    int32_t x, y, w, h;
} UwpRect;

/* The largest rectangle of the frame's shape centred in the window: bars at
   the sides or at top and bottom, never both. */
static inline UwpRect UwpLetterbox(uint32_t windowW, uint32_t windowH, uint32_t frameW, uint32_t frameH) {
    UwpRect rect = {0, 0, (int32_t)windowW, (int32_t)windowH};
    if (windowW == 0 || windowH == 0 || frameW == 0 || frameH == 0)
        return rect;
    if ((uint64_t)windowW * frameH > (uint64_t)windowH * frameW) {
        rect.w = (int32_t)(((uint64_t)windowH * frameW + frameH / 2) / frameH);
        rect.x = ((int32_t)windowW - rect.w) / 2;
    } else {
        rect.h = (int32_t)(((uint64_t)windowW * frameH + frameW / 2) / frameW);
        rect.y = ((int32_t)windowH - rect.h) / 2;
    }
    return rect;
}

/* A window pixel to the frame pixel under it, clamped to the frame. */
static inline void UwpWindowToFrame(UwpRect rect, uint32_t frameW, uint32_t frameH,
                                    int32_t x, int32_t y, int32_t *fx, int32_t *fy) {
    int64_t mx = rect.w > 0 ? ((int64_t)(x - rect.x) * frameW) / rect.w : x;
    int64_t my = rect.h > 0 ? ((int64_t)(y - rect.y) * frameH) / rect.h : y;
    if (mx < 0)
        mx = 0;
    if (my < 0)
        my = 0;
    if (frameW && mx >= (int64_t)frameW)
        mx = (int64_t)frameW - 1;
    if (frameH && my >= (int64_t)frameH)
        my = (int64_t)frameH - 1;
    *fx = (int32_t)mx;
    *fy = (int32_t)my;
}
