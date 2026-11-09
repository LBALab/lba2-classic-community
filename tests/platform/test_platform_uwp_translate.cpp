/* The UWP backend's Windows-free parts (LIB386/PLATFORM/UWP/TRANSLATE.H and
   QUEUE.H), checked on any host. Expected values come from outside the header: PC set-1
   scan codes and Windows virtual keys as documented, the Windows.Gaming.Input
   GamepadButtons values, and, when SDL3 is in the build, SDL's own key names
   and US-layout key codes, which the other backends report. */

#include "../test_harness.h"
#include "../../LIB386/PLATFORM/UWP/QUEUE.H"
#include "../../LIB386/PLATFORM/UWP/TRANSLATE.H"

#include <string.h>

#ifdef UWP_TEST_WITH_SDL3
#include <SDL3/SDL.h>
#endif

static void test_scan_codes() {
    /* Plain set-1 codes. */
    ASSERT_EQ_INT(PLATFORM_SCANCODE_ESCAPE, UwpKeyScancode(0x1B, 0x01, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_1, UwpKeyScancode(0x31, 0x02, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_0, UwpKeyScancode(0x30, 0x0B, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_Q, UwpKeyScancode(0x51, 0x10, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_RETURN, UwpKeyScancode(0x0D, 0x1C, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_LCTRL, UwpKeyScancode(0xA2, 0x1D, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_A, UwpKeyScancode(0x41, 0x1E, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_GRAVE, UwpKeyScancode(0xC0, 0x29, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_LSHIFT, UwpKeyScancode(0xA0, 0x2A, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_Z, UwpKeyScancode(0x5A, 0x2C, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_RSHIFT, UwpKeyScancode(0xA1, 0x36, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_LALT, UwpKeyScancode(0xA4, 0x38, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_SPACE, UwpKeyScancode(0x20, 0x39, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_F1, UwpKeyScancode(0x70, 0x3B, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_F10, UwpKeyScancode(0x79, 0x44, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_F11, UwpKeyScancode(0x7A, 0x57, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_F12, UwpKeyScancode(0x7B, 0x58, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_NONUSBACKSLASH, UwpKeyScancode(0xE2, 0x56, false));

    /* The same codes mean different keys with and without the E0 prefix. */
    ASSERT_EQ_INT(PLATFORM_SCANCODE_KP_8, UwpKeyScancode(0x68, 0x48, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_UP, UwpKeyScancode(0x26, 0x48, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_KP_4, UwpKeyScancode(0x64, 0x4B, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_LEFT, UwpKeyScancode(0x25, 0x4B, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_RIGHT, UwpKeyScancode(0x27, 0x4D, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_DOWN, UwpKeyScancode(0x28, 0x50, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_RETURN, UwpKeyScancode(0x0D, 0x1C, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_KP_ENTER, UwpKeyScancode(0x0D, 0x1C, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_RCTRL, UwpKeyScancode(0xA3, 0x1D, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_RALT, UwpKeyScancode(0xA5, 0x38, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_SLASH, UwpKeyScancode(0xBF, 0x35, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_KP_DIVIDE, UwpKeyScancode(0x6F, 0x35, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_KP_MULTIPLY, UwpKeyScancode(0x6A, 0x37, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_PRINTSCREEN, UwpKeyScancode(0x2C, 0x37, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_HOME, UwpKeyScancode(0x24, 0x47, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_DELETE, UwpKeyScancode(0x2E, 0x53, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_KP_PERIOD, UwpKeyScancode(0x6E, 0x53, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_LGUI, UwpKeyScancode(0x5B, 0x5B, true));

    /* Pause and Num Lock share 0x45; the virtual key decides. */
    ASSERT_EQ_INT(PLATFORM_SCANCODE_PAUSE, UwpKeyScancode(0x13, 0x45, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_PAUSE, UwpKeyScancode(0x13, 0x45, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_NUMLOCKCLEAR, UwpKeyScancode(0x90, 0x45, true));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_NUMLOCKCLEAR, UwpKeyScancode(0x90, 0x45, false));
}

static void test_virtual_key_fallback() {
    /* No scan code (an on-screen keyboard): placed by virtual key. */
    ASSERT_EQ_INT(PLATFORM_SCANCODE_A, UwpKeyScancode(0x41, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_Z, UwpKeyScancode(0x5A, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_0, UwpKeyScancode(0x30, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_9, UwpKeyScancode(0x39, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_RETURN, UwpKeyScancode(0x0D, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_ESCAPE, UwpKeyScancode(0x1B, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_BACKSPACE, UwpKeyScancode(0x08, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_LEFT, UwpKeyScancode(0x25, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_DOWN, UwpKeyScancode(0x28, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_F12, UwpKeyScancode(0x7B, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_LSHIFT, UwpKeyScancode(0x10, 0, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_UNKNOWN, UwpKeyScancode(0xFF, 0, false));
    /* An unknown extended code falls back the same way. */
    ASSERT_EQ_INT(PLATFORM_SCANCODE_UP, UwpKeyScancode(0x26, 0x7F, true));
}

static void test_gamepad_keys_dropped() {
    /* VK_GAMEPAD_A (0xC3) .. VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT (0xDA) and the
       navigation keys (0x88 .. 0x8F): the pad is read directly. */
    for (uint32_t vk = 0xC3; vk <= 0xDA; ++vk)
        ASSERT_EQ_INT(PLATFORM_SCANCODE_UNKNOWN, UwpKeyScancode(vk, 0, false));
    for (uint32_t vk = 0x88; vk <= 0x8F; ++vk)
        ASSERT_EQ_INT(PLATFORM_SCANCODE_UNKNOWN, UwpKeyScancode(vk, 0, false));
    /* Even with a scan code attached. */
    ASSERT_EQ_INT(PLATFORM_SCANCODE_UNKNOWN, UwpKeyScancode(0xC3, 0x1C, false));
    /* The neighbours are keys. */
    ASSERT_EQ_INT(PLATFORM_SCANCODE_GRAVE, UwpKeyScancode(0xC0, 0x29, false));
    ASSERT_EQ_INT(PLATFORM_SCANCODE_LEFTBRACKET, UwpKeyScancode(0xDB, 0x1A, false));
}

static void test_modifiers() {
    ASSERT_EQ_INT(PLATFORM_KMOD_LSHIFT, UwpModifierBit(PLATFORM_SCANCODE_LSHIFT));
    ASSERT_EQ_INT(PLATFORM_KMOD_RSHIFT, UwpModifierBit(PLATFORM_SCANCODE_RSHIFT));
    ASSERT_EQ_INT(PLATFORM_KMOD_LCTRL, UwpModifierBit(PLATFORM_SCANCODE_LCTRL));
    ASSERT_EQ_INT(PLATFORM_KMOD_RCTRL, UwpModifierBit(PLATFORM_SCANCODE_RCTRL));
    ASSERT_EQ_INT(PLATFORM_KMOD_LALT, UwpModifierBit(PLATFORM_SCANCODE_LALT));
    ASSERT_EQ_INT(PLATFORM_KMOD_RALT, UwpModifierBit(PLATFORM_SCANCODE_RALT));
    ASSERT_EQ_INT(PLATFORM_KMOD_LGUI, UwpModifierBit(PLATFORM_SCANCODE_LGUI));
    ASSERT_EQ_INT(PLATFORM_KMOD_RGUI, UwpModifierBit(PLATFORM_SCANCODE_RGUI));
    ASSERT_EQ_INT(0, UwpModifierBit(PLATFORM_SCANCODE_A));
}

static void test_keycodes() {
    ASSERT_EQ_UINT('a', UwpKeycode(PLATFORM_SCANCODE_A, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT('A', UwpKeycode(PLATFORM_SCANCODE_A, PLATFORM_KMOD_LSHIFT));
    ASSERT_EQ_UINT('z', UwpKeycode(PLATFORM_SCANCODE_Z, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT('1', UwpKeycode(PLATFORM_SCANCODE_1, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT('!', UwpKeycode(PLATFORM_SCANCODE_1, PLATFORM_KMOD_RSHIFT));
    ASSERT_EQ_UINT('0', UwpKeycode(PLATFORM_SCANCODE_0, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(')', UwpKeycode(PLATFORM_SCANCODE_0, PLATFORM_KMOD_LSHIFT));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_RETURN, UwpKeycode(PLATFORM_SCANCODE_RETURN, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_ESCAPE, UwpKeycode(PLATFORM_SCANCODE_ESCAPE, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_BACKSPACE, UwpKeycode(PLATFORM_SCANCODE_BACKSPACE, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_TAB, UwpKeycode(PLATFORM_SCANCODE_TAB, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_SPACE, UwpKeycode(PLATFORM_SCANCODE_SPACE, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_DELETE, UwpKeycode(PLATFORM_SCANCODE_DELETE, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT('/', UwpKeycode(PLATFORM_SCANCODE_SLASH, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT('?', UwpKeycode(PLATFORM_SCANCODE_SLASH, PLATFORM_KMOD_LSHIFT));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_UP, UwpKeycode(PLATFORM_SCANCODE_UP, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_F1, UwpKeycode(PLATFORM_SCANCODE_F1, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(PLATFORM_KEYCODE_KP_ENTER, UwpKeycode(PLATFORM_SCANCODE_KP_ENTER, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(0, UwpKeycode(PLATFORM_SCANCODE_UNKNOWN, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(0, UwpKeycode(PLATFORM_SCANCODE_COUNT, PLATFORM_KMOD_NONE));
    ASSERT_EQ_UINT(0, UwpKeycode(-1, PLATFORM_KMOD_NONE));
}

static void test_names() {
    ASSERT_TRUE(strcmp("A", UwpScancodeName(PLATFORM_SCANCODE_A)) == 0);
    ASSERT_TRUE(strcmp("Z", UwpScancodeName(PLATFORM_SCANCODE_Z)) == 0);
    ASSERT_TRUE(strcmp("Return", UwpScancodeName(PLATFORM_SCANCODE_RETURN)) == 0);
    ASSERT_TRUE(strcmp("F12", UwpScancodeName(PLATFORM_SCANCODE_F12)) == 0);
    ASSERT_TRUE(strcmp("Up", UwpScancodeName(PLATFORM_SCANCODE_UP)) == 0);
    ASSERT_TRUE(strcmp("Keypad .", UwpScancodeName(PLATFORM_SCANCODE_KP_PERIOD)) == 0);
    ASSERT_TRUE(strcmp("Application", UwpScancodeName(PLATFORM_SCANCODE_APPLICATION)) == 0);
    ASSERT_TRUE(strcmp("Left Ctrl", UwpScancodeName(PLATFORM_SCANCODE_LCTRL)) == 0);
    ASSERT_TRUE(strcmp("Right GUI", UwpScancodeName(PLATFORM_SCANCODE_RGUI)) == 0);
    ASSERT_TRUE(strcmp("", UwpScancodeName(PLATFORM_SCANCODE_UNKNOWN)) == 0);
    ASSERT_TRUE(strcmp("", UwpScancodeName(PLATFORM_SCANCODE_COUNT)) == 0);
}

#ifdef UWP_TEST_WITH_SDL3
/* Every key the UWP backend can report, against SDL3, which the desktop
   build reports: same name, same unshifted key code. SDL3 with no video uses
   its default US keymap, which has no Shift layer (Shift+A gives 'a'), so
   shifted codes are checked against the US layout in test_keycodes instead. */
static void test_against_sdl3() {
    int named = 0;
    for (int sc = 1; sc < PLATFORM_SCANCODE_COUNT; ++sc) {
        const char *ours = UwpScancodeName(sc);
        if (!*ours)
            continue;
        ++named;
        const char *sdl = SDL_GetScancodeName((SDL_Scancode)sc);
        if (strcmp(ours, sdl) != 0)
            printf("  scancode %d: ours '%s', SDL '%s'\n", sc, ours, sdl);
        ASSERT_TRUE(strcmp(ours, sdl) == 0);
        ASSERT_EQ_UINT((uint32_t)SDL_GetKeyFromScancode((SDL_Scancode)sc, SDL_KMOD_NONE, true),
                       UwpKeycode(sc, PLATFORM_KMOD_NONE));
    }
    /* Everything the scan code tables produce has a name. */
    ASSERT_TRUE(named >= 100);
    for (uint32_t scan = 0; scan < 0x80; ++scan)
        for (int e = 0; e < 2; ++e) {
            const int32_t sc = UwpKeyScancode(0x07 /* unassigned VK */, scan, e != 0);
            if (sc != PLATFORM_SCANCODE_UNKNOWN)
                ASSERT_TRUE(*UwpScancodeName(sc) != '\0');
        }
}
#endif

static void test_pad_buttons() {
    /* Windows.Gaming.Input.GamepadButtons. */
    const uint32_t Menu = 0x1, View = 0x2, A = 0x4, B = 0x8, X = 0x10, Y = 0x20, DPadUp = 0x40,
                   DPadDown = 0x80, DPadLeft = 0x100, DPadRight = 0x200, LeftShoulder = 0x400,
                   RightShoulder = 0x800, LeftThumbstick = 0x1000, RightThumbstick = 0x2000;
    struct {
        PlatformGamepadButton button;
        uint32_t bit;
    } expected[] = {
        {PLATFORM_GAMEPAD_BUTTON_SOUTH, A},
        {PLATFORM_GAMEPAD_BUTTON_EAST, B},
        {PLATFORM_GAMEPAD_BUTTON_WEST, X},
        {PLATFORM_GAMEPAD_BUTTON_NORTH, Y},
        {PLATFORM_GAMEPAD_BUTTON_BACK, View},
        {PLATFORM_GAMEPAD_BUTTON_START, Menu},
        {PLATFORM_GAMEPAD_BUTTON_LEFT_STICK, LeftThumbstick},
        {PLATFORM_GAMEPAD_BUTTON_RIGHT_STICK, RightThumbstick},
        {PLATFORM_GAMEPAD_BUTTON_LEFT_SHOULDER, LeftShoulder},
        {PLATFORM_GAMEPAD_BUTTON_RIGHT_SHOULDER, RightShoulder},
        {PLATFORM_GAMEPAD_BUTTON_DPAD_UP, DPadUp},
        {PLATFORM_GAMEPAD_BUTTON_DPAD_DOWN, DPadDown},
        {PLATFORM_GAMEPAD_BUTTON_DPAD_LEFT, DPadLeft},
        {PLATFORM_GAMEPAD_BUTTON_DPAD_RIGHT, DPadRight},
    };
    const size_t count = sizeof expected / sizeof expected[0];
    for (size_t i = 0; i < count; ++i) {
        /* Its own bit presses it; every other bit leaves it up. */
        ASSERT_TRUE(UwpPadButton(expected[i].bit, expected[i].button));
        ASSERT_TRUE(!UwpPadButton(0x3FFF & ~expected[i].bit, expected[i].button));
    }
    /* The pad has no guide button an app can read. */
    ASSERT_TRUE(!UwpPadButton(0xFFFFFFFF, PLATFORM_GAMEPAD_BUTTON_GUIDE));
    ASSERT_TRUE(!UwpPadButton(0xFFFFFFFF, PLATFORM_GAMEPAD_BUTTON_COUNT));
    ASSERT_TRUE(!UwpPadButton(0xFFFFFFFF, (PlatformGamepadButton)-1));
}

static void test_pad_axes() {
    ASSERT_EQ_INT(0, UwpPadUnit(0.0, false));
    ASSERT_EQ_INT(32767, UwpPadUnit(1.0, false));
    ASSERT_EQ_INT(-32768, UwpPadUnit(-1.0, false));
    ASSERT_EQ_INT(16383, UwpPadUnit(0.5, false));
    ASSERT_EQ_INT(-16384, UwpPadUnit(-0.5, false));
    /* Y is up in WinRT and down in the engine: stick up reads negative. */
    ASSERT_EQ_INT(-32768, UwpPadUnit(1.0, true));
    ASSERT_EQ_INT(32767, UwpPadUnit(-1.0, true));
    /* Out of range and NaN. */
    ASSERT_EQ_INT(32767, UwpPadUnit(1.5, false));
    ASSERT_EQ_INT(-32768, UwpPadUnit(-7.0, false));
    const double zero = 0.0;
    ASSERT_EQ_INT(0, UwpPadUnit(zero / zero, false));
    /* Monotonic across the range. */
    int16_t previous = UwpPadUnit(-1.0, false);
    for (int i = -999; i <= 1000; ++i) {
        const int16_t value = UwpPadUnit(i / 1000.0, false);
        ASSERT_TRUE(value >= previous);
        previous = value;
    }
}

static void test_pad_events() {
    const int16_t rest[PLATFORM_GAMEPAD_AXIS_COUNT] = {0, 0, 0, 0, 0, 0};
    PlatformEvent out[UWP_PAD_EVENTS_MAX];
    /* Nothing changed: nothing to say. */
    ASSERT_EQ_INT(0, UwpPadEvents(7, 0x0004, rest, 0x0004, rest, out));
    /* A newly down; B already held; X released (no event: the contract has none). */
    ASSERT_EQ_INT(1, UwpPadEvents(7, 0x0008 | 0x0010, rest, 0x0004 | 0x0008, rest, out));
    ASSERT_EQ_INT(PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN, out[0].type);
    ASSERT_EQ_UINT(7, out[0].gdevice.which);
    /* Two axes moved, in axis order, with their new values. */
    int16_t moved[PLATFORM_GAMEPAD_AXIS_COUNT] = {0, 0, 0, 0, 0, 0};
    moved[PLATFORM_GAMEPAD_AXIS_LEFTY] = -32768;
    moved[PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER] = 100;
    ASSERT_EQ_INT(2, UwpPadEvents(3, 0, rest, 0, moved, out));
    ASSERT_EQ_INT(PLATFORM_EVENT_GAMEPAD_AXIS_MOTION, out[0].type);
    ASSERT_EQ_UINT(3, out[0].gaxis.which);
    ASSERT_EQ_INT(PLATFORM_GAMEPAD_AXIS_LEFTY, out[0].gaxis.axis);
    ASSERT_EQ_INT(-32768, out[0].gaxis.value);
    ASSERT_EQ_INT(PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER, out[1].gaxis.axis);
    ASSERT_EQ_INT(100, out[1].gaxis.value);
    /* Everything at once fits the buffer: every mapped button and every axis. */
    int16_t all[PLATFORM_GAMEPAD_AXIS_COUNT] = {1, 1, 1, 1, 1, 1};
    int mapped = 0;
    for (int b = 0; b < PLATFORM_GAMEPAD_BUTTON_COUNT; ++b)
        mapped += UwpPadButton(0xFFFF, (PlatformGamepadButton)b);
    ASSERT_EQ_INT(mapped + PLATFORM_GAMEPAD_AXIS_COUNT, UwpPadEvents(1, 0, rest, 0xFFFF, all, out));
}

static UwpQueue s_queue;

static PlatformEvent Event(PlatformEventType type, int32_t scancode) {
    PlatformEvent event;
    memset(&event, 0, sizeof event);
    event.type = type;
    event.key.scancode = scancode;
    return event;
}

static void test_queue_order_and_text() {
    UwpQueueClear(&s_queue);
    PlatformEvent event = Event(PLATFORM_EVENT_KEY_DOWN, 4);
    ASSERT_TRUE(UwpQueuePush(&s_queue, &event, NULL));
    event = Event(PLATFORM_EVENT_TEXT_INPUT, 0);
    ASSERT_TRUE(UwpQueuePush(&s_queue, &event, "\xC3\xA9"));
    event = Event(PLATFORM_EVENT_TEXT_INPUT, 0);
    ASSERT_TRUE(UwpQueuePush(&s_queue, &event, "longer than the slot"));
    PlatformEvent out;
    ASSERT_TRUE(UwpQueuePoll(&s_queue, &out));
    ASSERT_EQ_INT(PLATFORM_EVENT_KEY_DOWN, out.type);
    ASSERT_EQ_INT(4, out.key.scancode);
    ASSERT_TRUE(UwpQueuePoll(&s_queue, &out));
    ASSERT_EQ_INT(PLATFORM_EVENT_TEXT_INPUT, out.type);
    ASSERT_TRUE(strcmp(out.text.text, "\xC3\xA9") == 0);
    ASSERT_TRUE(UwpQueuePoll(&s_queue, &out));
    ASSERT_EQ_INT(UWP_TEXT_SIZE - 1, (int)strlen(out.text.text));
    ASSERT_TRUE(!UwpQueuePoll(&s_queue, &out));
    /* The ring wraps. */
    for (int round = 0; round < 3 * UWP_QUEUE_SIZE; ++round) {
        event = Event(PLATFORM_EVENT_KEY_UP, round);
        ASSERT_TRUE(UwpQueuePush(&s_queue, &event, NULL));
        ASSERT_TRUE(UwpQueuePoll(&s_queue, &out));
        ASSERT_EQ_INT(round, out.key.scancode);
    }
}

static void test_queue_full_keeps_quit() {
    UwpQueueClear(&s_queue);
    PlatformEvent event;
    for (int i = 0; i < UWP_QUEUE_SIZE; ++i) {
        event = Event(PLATFORM_EVENT_GAMEPAD_AXIS_MOTION, i);
        ASSERT_TRUE(UwpQueuePush(&s_queue, &event, NULL));
    }
    /* Full: an ordinary event is dropped, the quit is kept. */
    event = Event(PLATFORM_EVENT_KEY_DOWN, 1);
    ASSERT_TRUE(!UwpQueuePush(&s_queue, &event, NULL));
    event = Event(PLATFORM_EVENT_QUIT, 0);
    ASSERT_TRUE(UwpQueuePush(&s_queue, &event, NULL));
    PlatformEvent out;
    int quits = 0, others = 0;
    while (UwpQueuePoll(&s_queue, &out)) {
        if (out.type == PLATFORM_EVENT_QUIT)
            ++quits;
        else
            ++others;
        ASSERT_TRUE(quits == 0 || others == UWP_QUEUE_SIZE); /* the quit comes last */
    }
    ASSERT_EQ_INT(1, quits);
    ASSERT_EQ_INT(UWP_QUEUE_SIZE, others);
    /* Delivered once; a cleared queue holds no pending quit. */
    ASSERT_TRUE(!UwpQueuePoll(&s_queue, &out));
    for (int i = 0; i < UWP_QUEUE_SIZE; ++i)
        UwpQueuePush(&s_queue, &event, NULL);
    UwpQueuePush(&s_queue, &event, NULL);
    UwpQueueClear(&s_queue);
    ASSERT_TRUE(!UwpQueuePoll(&s_queue, &out));
}

static void test_letterbox() {
    /* The game's auto mode on a 1080p TV: bars at the sides. */
    UwpRect r = UwpLetterbox(1920, 1080, 848, 480);
    ASSERT_EQ_INT(6, r.x);
    ASSERT_EQ_INT(0, r.y);
    ASSERT_EQ_INT(1908, r.w);
    ASSERT_EQ_INT(1080, r.h);
    r = UwpLetterbox(1920, 1080, 640, 480);
    ASSERT_EQ_INT(240, r.x);
    ASSERT_EQ_INT(1440, r.w);
    ASSERT_EQ_INT(1080, r.h);
    /* A tall window: bars at top and bottom. */
    r = UwpLetterbox(1200, 1600, 640, 480);
    ASSERT_EQ_INT(0, r.x);
    ASSERT_EQ_INT(350, r.y);
    ASSERT_EQ_INT(1200, r.w);
    ASSERT_EQ_INT(900, r.h);
    /* Same shape: no bars. */
    r = UwpLetterbox(1280, 960, 640, 480);
    ASSERT_EQ_INT(0, r.x);
    ASSERT_EQ_INT(0, r.y);
    ASSERT_EQ_INT(1280, r.w);
    ASSERT_EQ_INT(960, r.h);
    /* Degenerate sizes give the whole window. */
    r = UwpLetterbox(1920, 1080, 0, 480);
    ASSERT_EQ_INT(1920, r.w);
    ASSERT_EQ_INT(1080, r.h);
    /* 4K, with room for 64-bit products. */
    r = UwpLetterbox(3840, 2160, 640, 480);
    ASSERT_EQ_INT(480, r.x);
    ASSERT_EQ_INT(2880, r.w);
}

static void test_window_to_frame() {
    const UwpRect r = UwpLetterbox(1920, 1080, 640, 480); /* x 240, w 1440 */
    int32_t fx, fy;
    UwpWindowToFrame(r, 640, 480, 240, 0, &fx, &fy);
    ASSERT_EQ_INT(0, fx);
    ASSERT_EQ_INT(0, fy);
    UwpWindowToFrame(r, 640, 480, 1679, 1079, &fx, &fy);
    ASSERT_EQ_INT(639, fx);
    ASSERT_EQ_INT(479, fy);
    UwpWindowToFrame(r, 640, 480, 960, 540, &fx, &fy);
    ASSERT_EQ_INT(320, fx);
    ASSERT_EQ_INT(240, fy);
    /* In the bars: clamped to the nearest edge. */
    UwpWindowToFrame(r, 640, 480, 10, 540, &fx, &fy);
    ASSERT_EQ_INT(0, fx);
    UwpWindowToFrame(r, 640, 480, 1900, 540, &fx, &fy);
    ASSERT_EQ_INT(639, fx);
    UwpWindowToFrame(r, 640, 480, -50, -50, &fx, &fy);
    ASSERT_EQ_INT(0, fx);
    ASSERT_EQ_INT(0, fy);
}

int main() {
    RUN_TEST(test_scan_codes);
    RUN_TEST(test_virtual_key_fallback);
    RUN_TEST(test_gamepad_keys_dropped);
    RUN_TEST(test_modifiers);
    RUN_TEST(test_keycodes);
    RUN_TEST(test_names);
#ifdef UWP_TEST_WITH_SDL3
    RUN_TEST(test_against_sdl3);
#endif
    RUN_TEST(test_pad_buttons);
    RUN_TEST(test_pad_axes);
    RUN_TEST(test_pad_events);
    RUN_TEST(test_queue_order_and_text);
    RUN_TEST(test_queue_full_keeps_quit);
    RUN_TEST(test_letterbox);
    RUN_TEST(test_window_to_frame);
    TEST_SUMMARY();
    return test_failures != 0;
}
